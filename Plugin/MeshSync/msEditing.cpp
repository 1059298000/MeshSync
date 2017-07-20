#include "pch.h"
#include "msEditing.h"
#ifdef muEnableAMP
    #include "MeshUtils/ampmath.h"
#endif


namespace ms {

void ProjectNormals(ms::Mesh& dst, ms::Mesh& src, EditFlags flags)
{
    dst.flags.has_normals = 1;
    dst.refine_settings.flags.gen_normals_with_smooth_angle = 0;

    // just copy normals if topology is identical
    if (src.indices == dst.indices) {
        ms::MeshRefineSettings rs;
        rs.flags.gen_normals_with_smooth_angle = 1;
        rs.flags.flip_normals = 1;
        rs.smooth_angle = src.refine_settings.smooth_angle;
        src.refine(rs);
        dst.normals = src.normals;
        return;
    }

    bool use_gpu = false;
#ifdef muEnableAMP
    use_gpu = apm::device_available() && flags & msEF_PreferGPU;
#endif
#ifdef msEnableProfiling
    auto tbegin = Now();
    static int s_count;
    if (++s_count % 2 == 0) { use_gpu = false; }
#endif

    RawVector<float> soa[9]; // flattened + SoA-nized vertices (faster on CPU)

    parallel_invoke([&]() {
            ms::MeshRefineSettings rs;
            rs.flags.triangulate = 1;
            rs.flags.gen_normals_with_smooth_angle = 1;
            rs.flags.flip_normals = 1;
            rs.smooth_angle = src.refine_settings.smooth_angle;
            src.refine(rs);

            // make optimal vertex data
            int num_triangles = (int)src.indices.size() / 3;
            if (!use_gpu) {
                // flatten + SoA-nize
                for (int i = 0; i < 9; ++i) {
                    soa[i].resize(num_triangles);
                }
                for (int ti = 0; ti < num_triangles; ++ti) {
                    for (int i = 0; i < 3; ++i) {
                        ms::float3 p = src.points[src.indices[ti * 3 + i]];
                        soa[i * 3 + 0][ti] = p.x;
                        soa[i * 3 + 1][ti] = p.y;
                        soa[i * 3 + 2][ti] = p.z;
                    }
                }
            }
        },
        [&]() {
            ms::MeshRefineSettings rs;
            rs.flags.no_reindexing = 1;
            rs.flags.gen_normals_with_smooth_angle = 1;
            rs.flags.flip_normals = 1;
            rs.smooth_angle = dst.refine_settings.smooth_angle;
            dst.refine(rs);
        }
    );

#ifdef msEnableProfiling
    auto tendgennormal = Now();
#endif

    int num_triangles = (int)src.indices.size() / 3;
    int num_rays = (int)dst.normals.size();
    bool is_normal_indexed = dst.normals.size() == dst.points.size();
#ifdef muEnableAMP
    if (use_gpu) {
        using namespace apm;

        array_view<const float_3> vpoints((int)src.points.size(), (float_3*)src.points.data());
        array_view<float_4> vpoints1(num_triangles); // flattened vertices
        array_view<float_4> vpoints2(num_triangles); // note: float_4 is a bit faster than float_3 to access
        array_view<float_4> vpoints3(num_triangles); // 
        array_view<const float_3> vnormals((int)src.normals.size(), (const float_3*)src.normals.data());
        array_view<const int_3> vindices(num_triangles, (const int_3*)src.indices.data());

        array_view<const float_3> vrpos((int)dst.points.size(), (const float_3*)dst.points.data());
        array_view<const int> vrindices((int)dst.indices.size(), (const int*)dst.indices.data());
        array_view<float_3> vresult(num_rays, (float_3*)dst.normals.data()); // inout

        // make flattened vertex data
        parallel_for_each(vpoints1.extent, [=](index<1> ti) restrict(amp)
        {
            int_3 idx = vindices[ti];
            (float_3&)vpoints1[ti] = vpoints[idx.x];
            (float_3&)vpoints2[ti] = vpoints[idx.y];
            (float_3&)vpoints3[ti] = vpoints[idx.z];
        });

        // do projection
        parallel_for_each(vresult.extent, [=](index<1> ri) restrict(amp)
        {
            float_3 rpos = is_normal_indexed ? vrpos[ri] : vrpos[vrindices[ri]];
            float_3 rdir = vresult[ri];
            float distance = FLT_MAX;
            int hit = 0;

            for (int ti = 0; ti < num_triangles; ++ti) {
                float d;
                if (apm::ray_triangle_intersection(rpos, rdir,
                    vpoints1[ti], vpoints2[ti], vpoints3[ti], d))
                {
                    if (d < distance) {
                        distance = d;
                        hit = ti;
                    }
                }
            }

            if (distance < FLT_MAX) {
                int_3 idx = vindices[hit];
                float_3 result = apm::triangle_interpolation(
                    rpos + rdir * distance,
                    vpoints1[hit], vpoints2[hit], vpoints3[hit],
                    vnormals[idx.x], vnormals[idx.y], vnormals[idx.z]);
                vresult[ri] = normalize(result);
            }
        });
        vresult.synchronize();
    }
    else
#endif
    {
        parallel_for(0, num_rays, [&](int ri) {
            float3 rpos = is_normal_indexed ? dst.points[ri] : dst.points[dst.indices[ri]];
            float3 rdir = dst.normals[ri];
            int ti;
            float distance;
            int num_hit = ms::RayTrianglesIntersectionSoA(rpos, rdir,
                soa[0].data(), soa[1].data(), soa[2].data(),
                soa[3].data(), soa[4].data(), soa[5].data(),
                soa[6].data(), soa[7].data(), soa[8].data(),
                num_triangles, ti, distance);

            if (num_hit > 0) {
                float3 result = triangle_interpolation(
                    rpos + rdir * distance,
                    { soa[0][ti], soa[1][ti], soa[2][ti] },
                    { soa[3][ti], soa[4][ti], soa[5][ti] },
                    { soa[6][ti], soa[7][ti], soa[8][ti] },
                    src.normals[src.indices[ti * 3 + 0]],
                    src.normals[src.indices[ti * 3 + 1]],
                    src.normals[src.indices[ti * 3 + 2]]);
                dst.normals[ri] = normalize(result);
            }
        });
    }

#ifdef msEnableProfiling
    auto tend = Now();
    msLogInfo(
        "ProjectNormals (%s): %d rays, %d triangles, %.2fms (%.2fms for setup)\n",
        use_gpu ? "GPU" : "CPU",
        num_rays, num_triangles,
        float(tend - tbegin) / 1000000.0f,
        float(tendgennormal - tbegin) / 1000000.0f
    );
#endif
}

} // namespace ms
