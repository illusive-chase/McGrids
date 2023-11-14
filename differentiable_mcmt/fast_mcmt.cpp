#include "fast_mcmt.hpp"
#include "kdtree.hpp"
#include <tbb/tbb.h>

namespace GEO
{
    MCMT::MCMT()
    {
        GEO::initialize();
        delaunay_ = new PeriodicDelaunay3d(periodic_, 1.0);
        if (!periodic_)
        {
            delaunay_->set_keeps_infinite(true);
        }
    }

    MCMT::~MCMT()
    {
        delete delaunay_;
    }

    void MCMT::clear()
    {
        delete delaunay_;
        num_point_visited_ = 0;
        point_positions_.clear();
        point_values_.clear();
        point_errors_.clear();

        // create new delaunay
        delaunay_ = new PeriodicDelaunay3d(periodic_, 1.0);
        if (!periodic_)
        {
            delaunay_->set_keeps_infinite(true);
        }
    }

    std::vector<double> MCMT::get_grid_points()
    {
        std::vector<double> grid_points;
        for (index_t v = 0; v < delaunay_->nb_vertices(); v++)
        {
            grid_points.push_back(delaunay_->vertex_ptr(v)[0]);
            grid_points.push_back(delaunay_->vertex_ptr(v)[1]);
            grid_points.push_back(delaunay_->vertex_ptr(v)[2]);
        }
        return grid_points;
    }

    std::vector<int> MCMT::get_grids()
    {
        std::vector<int> v_indices;

        for (int i = 0; i < delaunay_->nb_finite_cells(); i++)
        {
            for (index_t lv = 0; lv < 4; ++lv)
            {
                int v = delaunay_->cell_vertex(i, lv);
                double x = delaunay_->vertex_ptr(v)[0];
                v_indices.push_back(int(v));
            }
        }
        return v_indices;
    }

    void MCMT::add_points(int num_points, double *point_positions, double *point_values)
    {
        num_point_visited_ = 0;
        int current_num_points = point_positions_.size() / 3;
        for (index_t i = 0; i < num_points; i++)
        {
            point_positions_.push_back(point_positions[i * 3]);
            point_positions_.push_back(point_positions[i * 3 + 1]);
            point_positions_.push_back(point_positions[i * 3 + 2]);
            point_values_.push_back(point_values[i]);
            point_errors_.push_back(1 / (abs(point_values[i]) + 1e-6));
        }
        delete delaunay_;
        delaunay_ = new PeriodicDelaunay3d(periodic_, 1.0);
        if (!periodic_)
        {
            delaunay_->set_keeps_infinite(true);
        }
        delaunay_->set_vertices(point_positions_.size() / 3, point_positions_.data());
        delaunay_->compute();

        for (int i = 0; i < point_positions_.size(); i++)
        {
            if (point_positions_[i] > max_bound)
            {
                max_bound = point_positions_[i];
            }
            if (point_positions_[i] < min_bound)
            {
                min_bound = point_positions_[i];
            }
        }


        for (int i = current_num_points; i < point_positions_.size() / 3; i++)
        {
            ConvexCell C;
            get_cell(i, C);
            double volume = C.volume();
            point_volumes_.push_back(volume);
            volume_changed_.push_back(false);
        }
        for (int i = current_num_points; i < point_positions_.size() / 3; i++)
        {
            delaunay_->get_incident_tets(i, W_);
            for (auto it = W_.begin(); it != W_.end(); it++)
            {
                // std::cout << "====================" << std::endl;

                for (int lv = 0; lv < 4; lv++)
                {
                    int point_id = delaunay_->cell_vertex(*it, lv);
                    // if (point_id > volume_changed_.size())
                    // {
                    //     std::cout << "point_id: " << point_id << std::endl;
                    //     std::cout << "volume_changed_.size(): " << volume_changed_.size() << std::endl;
                    //     std::cout << "Point.size(): " << point_positions_.size() / 3 << std::endl;
                    // }
                    if (point_id == -1)
                        continue;
                    volume_changed_[point_id] = true;
                }
            }
        }

        // std::cout << "nb vertices: " << delaunay_->nb_vertices() << std::endl;
        // std::cout << "nb point_volumes_: " << point_volumes_.size() << std::endl;

        for (int i = 0; i < point_positions_.size() / 3; i++)
        {
            if (volume_changed_[i])
            {
                ConvexCell C;
                get_cell(i, C);
                point_volumes_[i] = C.volume();
            }
        }
        // exit(0);
    }

    void MCMT::add_mid_points(int num_points, double *point_positions, double *point_values)
    {
        num_point_visited_ = point_positions_.size() / 3;
        for (index_t i = 0; i < num_points; i++)
        {
            point_positions_.push_back(point_positions[i * 3]);
            point_positions_.push_back(point_positions[i * 3 + 1]);
            point_positions_.push_back(point_positions[i * 3 + 2]);
            point_values_.push_back(point_values[i]);
            point_errors_.push_back(1 / (abs(point_values[i]) + 1e-6));
        }
        delete delaunay_;
        delaunay_ = new PeriodicDelaunay3d(periodic_, 1.0);
        if (!periodic_)
        {
            delaunay_->set_keeps_infinite(true);
        }
        delaunay_->set_vertices(point_positions_.size() / 3, point_positions_.data());
        delaunay_->compute();
        for (int i = 0; i < point_positions_.size(); i++)
        {
            if (point_positions_[i] > max_bound)
            {
                max_bound = point_positions_[i];
            }
            if (point_positions_[i] < min_bound)
            {
                min_bound = point_positions_[i];
            }
        }
    }

    std::vector<double> MCMT::interpolate(double *point1, double *point2, double sd1, double sd2)
    {
        double p1_x = point1[0];
        double p1_y = point1[1];
        double p1_z = point1[2];
        double p2_x = point2[0];
        double p2_y = point2[1];
        double p2_z = point2[2];
        double t = sd1 / ((sd1 - sd2));
        if (abs(sd1 - sd2) < 1e-6)
        {
            std::cout << "WARNING! SD1 == SD2" << std::endl;
            t = 0.5;
        }
        return std::vector<double>{p1_x + t * (p2_x - p1_x), p1_y + t * (p2_y - p1_y), p1_z + t * (p2_z - p1_z)};
    }

    std::vector<double> MCMT::sample_points_rejection(int num_points, double min_bound, double max_bound)
    {
        // compute density
        KDTree tree = KDTree(point_positions_.size() / 3, point_positions_.data(), point_errors_.data());
        int current_num_points = 0;
        int batch_size = 4096;
        std::vector<double> sampled_points;
        while (current_num_points < num_points)
        {
            std::vector<double> new_points;
            new_points.reserve(batch_size * 3);
            for (int i = 0; i < batch_size * 3; i++)
            {
                new_points.push_back(Numeric::random_float64() * (max_bound - min_bound) + min_bound);
            }
            std::vector<double> density = tree.compute_density(batch_size, new_points.data());
            double max_density = *std::max_element(density.begin(), density.end());
            for (int i = 0; i < batch_size; i++)
            {
                double threshold = Numeric::random_float64() * max_density;
                if (density[i] > threshold)
                {
                    sampled_points.push_back(new_points[i * 3]);
                    sampled_points.push_back(new_points[i * 3 + 1]);
                    sampled_points.push_back(new_points[i * 3 + 2]);
                    current_num_points++;
                    if (current_num_points == num_points)
                        break;
                }
            }
        }
        return sampled_points;
    }

    double MCMT::tetrahedronVolume(const std::vector<double> &coordinates)
    {
        // Check if there are enough coordinates
        if (coordinates.size() != 12)
        {
            std::cerr << "Error: Insufficient coordinates provided." << std::endl;
            return 0.0; // Return an appropriate value
        }

        // Extract coordinates of points A, B, C, and D
        double Ax = coordinates[0], Ay = coordinates[1], Az = coordinates[2];
        double Bx = coordinates[3], By = coordinates[4], Bz = coordinates[5];
        double Cx = coordinates[6], Cy = coordinates[7], Cz = coordinates[8];
        double Dx = coordinates[9], Dy = coordinates[10], Dz = coordinates[11];

        // Vector from A to B
        double ABx = Bx - Ax, ABy = By - Ay, ABz = Bz - Az;

        // Vector from A to C
        double ACx = Cx - Ax, ACy = Cy - Ay, ACz = Cz - Az;

        // Vector from A to D
        double ADx = Dx - Ax, ADy = Dy - Ay, ADz = Dz - Az;

        // Cross product of AC and AD
        double crossProduct_i = ACy * ADz - ACz * ADy;
        double crossProduct_j = ACz * ADx - ACx * ADz;
        double crossProduct_k = ACx * ADy - ACy * ADx;

        // Dot product of AB and the cross product of AC and AD
        double dotProduct = ABx * crossProduct_i + ABy * crossProduct_j + ABz * crossProduct_k;

        // Volume calculation
        double volume = std::abs(dotProduct) / 6.0;

        return volume;
    }

    std::vector<double> MCMT::compute_voronoi_error()
    {

        std::vector<double> voronoi_errors;

        for (int i = 0; i < delaunay_->nb_vertices(); i++)
        {
            double volume = point_volumes_[i];
            voronoi_errors.push_back(point_errors_[i] * volume);
        }

        return voronoi_errors;
    }

    std::vector<double> MCMT::compute_tet_error()
    {
        std::vector<double> tet_errors;
        tet_errors.resize(delaunay_->nb_finite_cells());

        tbb::parallel_for(tbb::blocked_range<int>(0, delaunay_->nb_finite_cells()),
                          [&](tbb::blocked_range<int> ti)
                          {
                              for (int i = ti.begin(); i < ti.end(); i++)
                              {
                                  double tet_density = 0;
                                  std::vector<double> point_coordinates;

                                  for (index_t lv = 0; lv < 4; ++lv)
                                  {
                                      int v = delaunay_->cell_vertex(i, lv);
                                      tet_density += point_errors_[v];
                                      for (int c = 0; c < 3; c++)
                                      {
                                          point_coordinates.push_back(point_positions_[v * 3 + c]);
                                      }
                                  }
                                  tet_errors[i] = tet_density * tetrahedronVolume(point_coordinates);
                              }
                          });
        return tet_errors;
    }

    int findBounds(const std::vector<double> &vec, float x)
    {
        int low = 0;
        int high = vec.size() - 1;

        while (low < high)
        {
            int mid = (low + high) / 2;

            if (vec[mid] < x)
            {
                low = mid + 1;
            }
            else
            {
                high = mid;
            }
        }

        if (low > 0 && std::abs(vec[low - 1] - x) < std::abs(vec[low] - x))
        {
            return low - 1;
        }
        else
        {
            return low;
        }
    }

    std::vector<double> MCMT::sample_polytope(int vor_index)
    {
        // vor_index = 10;
        std::ofstream output_mesh("cell_new.obj");
        ConvexCell C;
        get_cell(vor_index, C);
        vec3 g;
        g = C.barycenter();
        int index = 1;
        // For each vertex of the Voronoi cell
        // Start at 1 (vertex 0 is point at infinity)
        std::vector<std::vector<double>> tetrahedrons;

        for (index_t v = 1; v < C.nb_v(); ++v)
        {
            index_t t = C.vertex_triangle(v);
            //  Happens if a clipping plane did not
            // clip anything.
            if (t == VBW::END_OF_LIST)
            {
                continue;
            }

            //   Now iterate on the Voronoi vertices of the
            // Voronoi facet. In dual form this means iterating
            // on the triangles incident to vertex v
            vec3 P[3];
            index_t n = 0;

            do
            {
                std::vector<double> tetrahedron;

                //   Triangulate the Voronoi facet and send the
                // triangles to OpenGL/GLUP.
                if (n == 0)
                {
                    P[0] = C.triangle_point(VBW::ushort(t));
                }
                else if (n == 1)
                {
                    P[1] = C.triangle_point(VBW::ushort(t));
                }
                else
                {
                    P[2] = C.triangle_point(VBW::ushort(t));

                    for (index_t i = 0; i < 3; ++i)
                    {
                        tetrahedron.push_back(P[i].x);
                        tetrahedron.push_back(P[i].y);
                        tetrahedron.push_back(P[i].z);
                        output_mesh << "v " << P[i].x << " " << P[i].y << " " << P[i].z << std::endl;
                    }
                    tetrahedron.push_back(g.x);
                    tetrahedron.push_back(g.y);
                    tetrahedron.push_back(g.z);
                    tetrahedrons.push_back(tetrahedron);
                    output_mesh << "f " << index << "  " << index + 1 << " " << index + 2 << std::endl;
                    index += 3;
                    P[1] = P[2];
                }
                index_t lv = C.triangle_find_vertex(t, v);
                t = C.triangle_adjacent(t, (lv + 1) % 3);
                ++n;
            } while (t != C.vertex_triangle(v));
        }
        std::vector<double> tetrahedron_volumes;
        for (int i = 0; i < tetrahedrons.size(); i++)
        {
            tetrahedron_volumes.push_back(tetrahedronVolume(tetrahedrons[i]));
        }

        double tet_volume_sum = std::accumulate(tetrahedron_volumes.begin(), tetrahedron_volumes.end(), 0.0);

        for (int i = 0; i < tetrahedron_volumes.size(); i++)
        {
            tetrahedron_volumes[i] /= tet_volume_sum;
        }

        std::vector<double> cumsum;
        double current_sum = 0;
        for (int i = 0; i < tetrahedron_volumes.size(); i++)
        {
            current_sum += tetrahedron_volumes[i];
            cumsum.push_back(current_sum);
        }

        auto upper = std::upper_bound(cumsum.begin(), cumsum.end(), Numeric::random_float64());
        int tet_index = std::distance(cumsum.begin(), upper);
        return sample_tet(tetrahedrons[tet_index]);
    }

    std::vector<double> MCMT::sample_points_voronoi(const int num_points)
    {
        std::vector<double> voronoi_density = compute_voronoi_error();
        // std::cout << "voro_density size: " << voronoi_density.size() << std::endl;
        // std::cout << "nb vertices size: " << delaunay_->nb_vertices() << std::endl;

        double voronoi_density_sum = std::accumulate(voronoi_density.begin(), voronoi_density.end(), 0.0);

        for (int i = 0; i < voronoi_density.size(); i++)
        {
            voronoi_density[i] /= voronoi_density_sum;
        }

        std::vector<double> cumsum;
        double current_sum = 0;
        for (int i = 0; i < voronoi_density.size(); i++)
        {
            current_sum += voronoi_density[i];
            cumsum.push_back(current_sum);
        }

        std::vector<double> sample_points;
        for (int i = 0; i < num_points; i++)
        {
            auto upper = std::upper_bound(cumsum.begin(), cumsum.end(), Numeric::random_float64());
            int voro_index = std::distance(cumsum.begin(), upper);
            // std::cout << "voro_idx : " << voro_index << std::endl;
            std::vector<double> sampled_point = sample_polytope(voro_index);
            sample_points.push_back(sampled_point[0]);
            sample_points.push_back(sampled_point[1]);
            sample_points.push_back(sampled_point[2]);
        }

        return sample_points;
    }

    std::vector<double> MCMT::sample_tet(std::vector<double> point_positions)
    {
        double s = Numeric::random_float64();
        double t = Numeric::random_float64();
        double u = Numeric::random_float64();
        if (s + t > 1.0)
        {
            s = 1.0 - s;
            t = 1.0 - t;
        }
        if (t + u > 1.0)
        {
            double tmp = u;
            u = 1.0 - s - t;
            t = 1.0 - tmp;
        }
        else if (s + t + u > 1.0)
        {
            double tmp = u;
            u = s + t + u - 1.0;
            s = 1 - t - tmp;
        }
        double a = 1 - s - t - u;

        double x = a * point_positions[0] + s * point_positions[3] + t * point_positions[6] + u * point_positions[9];
        double y = a * point_positions[1] + s * point_positions[4] + t * point_positions[7] + u * point_positions[10];
        double z = a * point_positions[2] + s * point_positions[5] + t * point_positions[8] + u * point_positions[11];
        return std::vector<double>{x, y, z};
    }

    std::vector<double> MCMT::compute_face_mid_point(int num_points, const std::vector<double> &points)
    {
        double mid_point_x = 0;
        double mid_point_y = 0;
        double mid_point_z = 0;

        for (int i = 0; i < num_points; i++)
        {
            mid_point_x += points[i * 3];
            mid_point_y += points[i * 3 + 1];
            mid_point_z += points[i * 3 + 2];
        }
        mid_point_x /= num_points;
        mid_point_y /= num_points;
        mid_point_z /= num_points;
        return std::vector<double>{mid_point_x, mid_point_y, mid_point_z};
    }

    std::vector<double> MCMT::get_mid_points()
    {
        // std::cout << "num_point_visited_: " << num_point_visited_ << std::endl;
        // std::cout << "Total num points: " << point_positions_.size() / 3 << std::endl;
        // std::cout << "Get 5184 " << std::endl;

        // delaunay_->get_incident_tets(5184, W_);

        // delaunay_->get_incident_tets(5184, W_);
        // exit(0);
        tbb::concurrent_set<size_t> new_cells;
        std::cout << "num_point_visited2_: " << num_point_visited_ << std::endl;
        for (int i = num_point_visited_; i < point_positions_.size()/3 - 1; i++)
        {
            std::cout << "Before" << std::endl;
            std::cout << "i: " << i << std::endl;
            std::cout << "nb_finite_cells: " << delaunay_->nb_finite_cells() << std::endl;
            std::cout << "points: " << point_positions_.size()/3 << std::endl;
            std::cout << "nb_vertices: " << delaunay_->nb_vertices() << std::endl;


            // segmenation fault is caused by the following line
            delaunay_->get_incident_tets(i, W_);
            std::cout << "After" << std::endl;

            for(auto it=W_.begin(); it!=W_.end(); it++){
                if(*it < delaunay_->nb_finite_cells() && *it >= 0){
                    new_cells.insert(*it);
                }
                // else{
                //     std::cout << "WARNING! Cell index out of range!" << std::endl;
                //     std::cout << "cell index: " << *it << std::endl;
                // }

            }
        }
        std::cout << "num_point_visited123_: " << num_point_visited_ << std::endl;

        std::vector<int> new_cell_ids(new_cells.begin(), new_cells.end());
        tbb::concurrent_vector<std::vector<double>> sample_points;
        tbb::parallel_for(tbb::blocked_range<int>(0, new_cell_ids.size()),
                          [&](tbb::blocked_range<int> ti)
                          {
						  for(index_t i = ti.begin(); i < ti.end(); i++)
						  {
                            index_t t = new_cell_ids[i];
                            unsigned char index = 0;

							  std::vector<int> tri2v;
							  for (index_t lv = 0; lv < 4; ++lv)
							  {
								  int v = delaunay_->cell_vertex(t, lv);
								  if (v != -1){
									  tri2v.push_back(int(v));
                                  }
                                if(point_values_[v] < 0){
                                    index |= (1 << lv);
                                }
							  }
                            if(index == 0x00 || index == 0x0F){
                                continue;
                            }

                            std::vector<double> point_coordinates;

                                for (index_t lv = 0; lv < 4; ++lv)
                                {
                                    int v = delaunay_->cell_vertex(i, lv);
                                    for(int c=0; c<3; c++){
                                        point_coordinates.push_back(point_positions_[v*3+c]);
                                    }
                                }
                            double volume =  tetrahedronVolume(point_coordinates);
                            // if(volume < 1e-9){
                            //     std::cout << "WARNING! Volume is too small!" << std::endl;
                            // }
							  std::vector<double> intersection_points;
							  if (point_values_[tri2v[0]] * point_values_[tri2v[1]] < 0)
							  {
                                  std::vector<double> interpolated_point = interpolate(point_positions_.data() + tri2v[0] * 3, point_positions_.data() + tri2v[1] * 3, point_values_[tri2v[0]], point_values_[tri2v[1]]);
								  intersection_points.insert(intersection_points.end(), interpolated_point.begin(), interpolated_point.end());
							  }
							  if (point_values_[tri2v[0]] * point_values_[tri2v[2]] < 0)
							  {   std::vector<double> interpolated_point = interpolate(point_positions_.data() + tri2v[0] * 3, point_positions_.data() + tri2v[2] * 3, point_values_[tri2v[0]], point_values_[tri2v[2]]);
								  intersection_points.insert(intersection_points.end(), interpolated_point.begin(), interpolated_point.end());
							  }
							  if (point_values_[tri2v[0]] * point_values_[tri2v[3]] < 0)
							  {   std::vector<double> interpolated_point = interpolate(point_positions_.data() + tri2v[0] * 3, point_positions_.data() + tri2v[3] * 3, point_values_[tri2v[0]], point_values_[tri2v[3]]);
								  intersection_points.insert(intersection_points.end(), interpolated_point.begin(), interpolated_point.end());
							  }
							  if (point_values_[tri2v[1]] * point_values_[tri2v[2]] < 0)
							  {   std::vector<double> interpolated_point = interpolate(point_positions_.data() + tri2v[1] * 3, point_positions_.data() + tri2v[2] * 3, point_values_[tri2v[1]], point_values_[tri2v[2]]);
								  intersection_points.insert(intersection_points.end(), interpolated_point.begin(), interpolated_point.end());
							  }
							  if (point_values_[tri2v[1]] * point_values_[tri2v[3]] < 0)
							  {   std::vector<double> interpolated_point = interpolate(point_positions_.data() + tri2v[1] * 3, point_positions_.data() + tri2v[3] * 3, point_values_[tri2v[1]], point_values_[tri2v[3]]);
								  intersection_points.insert(intersection_points.end(), interpolated_point.begin(), interpolated_point.end());
							  }
							  if (point_values_[tri2v[2]] * point_values_[tri2v[3]] < 0)
							  {   std::vector<double> interpolated_point = interpolate(point_positions_.data() + tri2v[2] * 3, point_positions_.data() + tri2v[3] * 3, point_values_[tri2v[2]], point_values_[tri2v[3]]);
								  intersection_points.insert(intersection_points.end(), interpolated_point.begin(), interpolated_point.end());
							  }
							  if (intersection_points.size() != 0)
							  { 
								std::vector<double> mid_point = compute_face_mid_point(intersection_points.size()/3, intersection_points);
							  	sample_points.push_back(mid_point);
							  }
						  } });

        std::vector<double> new_points;
        new_points.reserve(sample_points.size() * 3);
        for (int i = 0; i < sample_points.size(); i++)
        {
            new_points.insert(new_points.end(), sample_points[i].begin(), sample_points[i].end());
        }
        return new_points;
    }

    void MCMT::get_cell(index_t v, ConvexCell &C)
    {
        delaunay_->copy_Laguerre_cell_from_Delaunay(v, C, W_);
        if (!periodic_)
        {
            C.clip_by_plane(vec4(1.0, 0.0, 0.0, max_bound));
            C.clip_by_plane(vec4(-1.0, 0.0, 0.0, -min_bound));
            C.clip_by_plane(vec4(0.0, 1.0, 0.0, max_bound));
            C.clip_by_plane(vec4(0.0, -1.0, 0.0, -min_bound));
            C.clip_by_plane(vec4(0.0, 0.0, 1.0, max_bound));
            C.clip_by_plane(vec4(0.0, 0.0, -1.0, -min_bound));
        }
        C.compute_geometry();
    }
    std::vector<double> MCMT::lloyd_relaxation(double *relaxed_point_positions, int num_points, int num_iter)
    {
        int current_num_points = point_positions_.size() / 3;
        // copy point_positions_ to new_points
        std::vector<double> new_points;
        new_points.reserve(point_positions_.size() + num_points * 3);
        for (int i = 0; i < point_positions_.size(); i++)
        {
            new_points.push_back(point_positions_[i]);
        }
        for (int i = 0; i < num_points * 3; i++)
        {
            new_points.push_back(relaxed_point_positions[i]);
        }
        delete delaunay_;
        delaunay_ = new PeriodicDelaunay3d(periodic_, 1.0);
        if (!periodic_)
        {
            delaunay_->set_keeps_infinite(true);
        }
        delaunay_->set_vertices(new_points.size() / 3, new_points.data());
        delaunay_->compute();
        ConvexCell C;

        for (int i = 0; i < num_iter; i++)
        {
            std::vector<double> new_new_points(new_points.size());
            for (index_t v = current_num_points; v < delaunay_->nb_vertices(); v++)
            {
                // std::cout << "V: " << v << std::endl;
                get_cell(v, C);
                vec3 g = C.barycenter();
                // std::cout << "g: " << g << std::endl;
                new_new_points[3 * v] = g.x;
                new_new_points[3 * v + 1] = g.y;
                new_new_points[3 * v + 2] = g.z;
            }
            for (index_t v = current_num_points; v < delaunay_->nb_vertices(); v++)
            {
                if (new_new_points[v] < min_bound)
                {
                    new_new_points[v] += (max_bound - min_bound);
                }
                if (new_new_points[v] > max_bound)
                {
                    new_new_points[v] -= (max_bound - min_bound);
                }
            }
            new_points.swap(new_new_points);
            delaunay_->set_vertices(new_points.size() / 3, new_points.data());
            delaunay_->compute();
        }
        std::vector<double> points_vec;
        points_vec.reserve(num_points * 3);

        for (int i = current_num_points * 3; i < current_num_points * 3 + num_points * 3; i++)
        {
            points_vec.push_back(new_points[i]);
        }
        return points_vec;
    }
    void MCMT::output_grid_points(std::string filename)
    {
        std::ofstream outfile(filename);
        for (int i = 0; i < point_positions_.size() / 3; i++)
        {
            outfile << "v " << point_positions_[i * 3] << " " << point_positions_[i * 3 + 1] << " " << point_positions_[i * 3 + 2] << std::endl;
        }
    }

    void MCMT::save_triangle_mesh(std::string filename)
    {

        std::vector<std::vector<double>> mesh_vertices;
        std::vector<std::vector<int>> mesh_faces;

        std::map<std::pair<int, int>, int> ev_map;
        int v_counter = 0;
        std::vector<int> intersection_cell;

        for (int i = 0; i < delaunay_->nb_finite_cells(); i++)
        {
            std::vector<int> v_indices;
            bool flag = false;
            for (index_t lv = 0; lv < 4; ++lv)
            {
                int v = delaunay_->cell_vertex(i, lv);
                v_indices.push_back(int(v));
            }

            if (point_values_[v_indices[0]] * point_values_[v_indices[1]] < 0)
            {
                flag = true;
                std::pair<int, int> e01 = (v_indices[0] < v_indices[1]) ? std::make_pair(v_indices[0], v_indices[1]) : std::make_pair(v_indices[1], v_indices[0]);
                if (ev_map.find(e01) == ev_map.end())
                {
                    std::vector<double> interpolated_point = interpolate(point_positions_.data() + v_indices[0] * 3, point_positions_.data() + v_indices[1] * 3, point_values_[v_indices[0]], point_values_[v_indices[1]]);
                    ev_map.insert({e01, mesh_vertices.size()});
                    mesh_vertices.push_back(interpolated_point);
                }
            }
            if (point_values_[v_indices[0]] * point_values_[v_indices[2]] < 0)
            {
                flag = true;
                std::pair<int, int> e02 = (v_indices[0] < v_indices[2]) ? std::make_pair(v_indices[0], v_indices[2]) : std::make_pair(v_indices[2], v_indices[0]);
                if (ev_map.find(e02) == ev_map.end())
                {
                    std::vector<double> interpolated_point = interpolate(point_positions_.data() + v_indices[0] * 3, point_positions_.data() + v_indices[2] * 3, point_values_[v_indices[0]], point_values_[v_indices[2]]);
                    ev_map.insert({e02, mesh_vertices.size()});
                    mesh_vertices.push_back(interpolated_point);
                }
            }
            if (point_values_[v_indices[0]] * point_values_[v_indices[3]] < 0)
            {
                flag = true;
                std::pair<int, int> e03 = (v_indices[0] < v_indices[3]) ? std::make_pair(v_indices[0], v_indices[3]) : std::make_pair(v_indices[3], v_indices[0]);
                if (ev_map.find(e03) == ev_map.end())
                {
                    std::vector<double> interpolated_point = interpolate(point_positions_.data() + v_indices[0] * 3, point_positions_.data() + v_indices[3] * 3, point_values_[v_indices[0]], point_values_[v_indices[3]]);
                    ev_map.insert({e03, mesh_vertices.size()});
                    mesh_vertices.push_back(interpolated_point);
                }
            }
            if (point_values_[v_indices[1]] * point_values_[v_indices[2]] < 0)
            {
                flag = true;
                std::pair<int, int> e12 = (v_indices[1] < v_indices[2]) ? std::make_pair(v_indices[1], v_indices[2]) : std::make_pair(v_indices[2], v_indices[1]);
                if (ev_map.find(e12) == ev_map.end())
                {
                    std::vector<double> interpolated_point = interpolate(point_positions_.data() + v_indices[1] * 3, point_positions_.data() + v_indices[2] * 3, point_values_[v_indices[1]], point_values_[v_indices[2]]);
                    ev_map.insert({e12, mesh_vertices.size()});
                    mesh_vertices.push_back(interpolated_point);
                }
            }
            if (point_values_[v_indices[1]] * point_values_[v_indices[3]] < 0)
            {
                flag = true;
                std::pair<int, int> e13 = (v_indices[1] < v_indices[3]) ? std::make_pair(v_indices[1], v_indices[3]) : std::make_pair(v_indices[3], v_indices[1]);
                if (ev_map.find(e13) == ev_map.end())
                {
                    std::vector<double> interpolated_point = interpolate(point_positions_.data() + v_indices[1] * 3, point_positions_.data() + v_indices[3] * 3, point_values_[v_indices[1]], point_values_[v_indices[3]]);
                    ev_map.insert({e13, mesh_vertices.size()});
                    mesh_vertices.push_back(interpolated_point);
                }
            }
            if (point_values_[v_indices[2]] * point_values_[v_indices[3]] < 0)
            {
                flag = true;
                std::pair<int, int> e23 = (v_indices[2] < v_indices[3]) ? std::make_pair(v_indices[2], v_indices[3]) : std::make_pair(v_indices[3], v_indices[2]);
                if (ev_map.find(e23) == ev_map.end())
                {
                    std::vector<double> interpolated_point = interpolate(point_positions_.data() + v_indices[2] * 3, point_positions_.data() + v_indices[3] * 3, point_values_[v_indices[2]], point_values_[v_indices[3]]);
                    ev_map.insert({e23, mesh_vertices.size()});
                    mesh_vertices.push_back(interpolated_point);
                }
            }

            if (flag)
            {
                intersection_cell.push_back(i);
            }
        }
        for (int i = 0; i < intersection_cell.size(); i++)
        {
            int cell_index = intersection_cell[i];

            unsigned char index = 0;
            std::vector<int> v_indices;
            for (index_t lv = 0; lv < 4; ++lv)
            {
                int v = delaunay_->cell_vertex(cell_index, lv);
                v_indices.push_back(int(v));

                if (point_values_[v] < 0)
                {
                    index |= (1 << lv);
                }
            }

            std::pair<int, int> e01 = (v_indices[0] < v_indices[1]) ? std::make_pair(v_indices[0], v_indices[1]) : std::make_pair(v_indices[1], v_indices[0]);
            std::pair<int, int> e02 = (v_indices[0] < v_indices[2]) ? std::make_pair(v_indices[0], v_indices[2]) : std::make_pair(v_indices[2], v_indices[0]);
            std::pair<int, int> e03 = (v_indices[0] < v_indices[3]) ? std::make_pair(v_indices[0], v_indices[3]) : std::make_pair(v_indices[3], v_indices[0]);
            std::pair<int, int> e12 = (v_indices[1] < v_indices[2]) ? std::make_pair(v_indices[1], v_indices[2]) : std::make_pair(v_indices[2], v_indices[1]);
            std::pair<int, int> e13 = (v_indices[1] < v_indices[3]) ? std::make_pair(v_indices[1], v_indices[3]) : std::make_pair(v_indices[3], v_indices[1]);
            std::pair<int, int> e23 = (v_indices[2] < v_indices[3]) ? std::make_pair(v_indices[2], v_indices[3]) : std::make_pair(v_indices[3], v_indices[2]);

            int v00, v10, v20, v01, v11, v21;

            // Case analysis
            switch (index)
            {
                // skip inside or outside situitions
            case 0x00:
            case 0x0F:
                break;
                // only vert 0 is inside
            case 0x01:
                v00 = ev_map[e01];
                v10 = ev_map[e03];
                v20 = ev_map[e02];

                mesh_faces.push_back({v00, v20, v10});
                break;

                // only vert 1 is inside
            case 0x02:
                v00 = ev_map[e01];
                v10 = ev_map[e12];
                v20 = ev_map[e13];

                mesh_faces.push_back({v00, v20, v10});
                break;

                // only vert 2 is inside
            case 0x04:
                v00 = ev_map[e02];
                v10 = ev_map[e23];
                v20 = ev_map[e12];

                mesh_faces.push_back({v00, v20, v10});
                break;

                // only vert 3 is inside
            case 0x08:
                v00 = ev_map[e13];
                v10 = ev_map[e23];
                v20 = ev_map[e03];

                mesh_faces.push_back({v00, v20, v10});
                break;

                // verts 0, 1 are inside
            case 0x03:
                v00 = ev_map[e03];
                v10 = ev_map[e02];
                v20 = ev_map[e13];

                mesh_faces.push_back({v00, v20, v10});
                v01 = ev_map[e02];
                v11 = ev_map[e12];
                v21 = ev_map[e13];

                mesh_faces.push_back({v01, v21, v11});
                break;

                // verts 0, 2 are inside
            case 0x05:
                v00 = ev_map[e03];
                v10 = ev_map[e12];
                v20 = ev_map[e01];

                mesh_faces.push_back({v00, v20, v10});
                v01 = ev_map[e12];
                v11 = ev_map[e03];
                v21 = ev_map[e23];

                mesh_faces.push_back({v01, v21, v11});
                break;

                // verts 0, 3 are inside
            case 0x09:
                v00 = ev_map[e01];
                v10 = ev_map[e13];
                v20 = ev_map[e02];

                mesh_faces.push_back({v00, v20, v10});
                v01 = ev_map[e13];
                v11 = ev_map[e23];
                v21 = ev_map[e02];

                mesh_faces.push_back({v01, v21, v11});
                break;

                // verts 1, 2 are inside
            case 0x06:
                v00 = ev_map[e01];
                v10 = ev_map[e02];
                v20 = ev_map[e13];

                mesh_faces.push_back({v00, v20, v10});
                v01 = ev_map[e13];
                v11 = ev_map[e02];
                v21 = ev_map[e23];
                mesh_faces.push_back({v01, v21, v11});
                break;

                // verts 2, 3 are inside
            case 0x0C:
                v00 = ev_map[e13];
                v10 = ev_map[e02];
                v20 = ev_map[e03];

                mesh_faces.push_back({v00, v20, v10});
                v01 = ev_map[e02];
                v11 = ev_map[e13];
                v21 = ev_map[e12];
                mesh_faces.push_back({v01, v21, v11});
                break;

                // verts 1, 3 are inside
            case 0x0A:
                v00 = ev_map[e03];
                v10 = ev_map[e01];
                v20 = ev_map[e12];

                mesh_faces.push_back({v00, v20, v10});
                v01 = ev_map[e12];
                v11 = ev_map[e23];
                v21 = ev_map[e03];
                mesh_faces.push_back({v01, v21, v11});
                break;

                // verts 0, 1, 2 are inside
            case 0x07:
                v00 = ev_map[e03];
                v10 = ev_map[e23];
                v20 = ev_map[e13];

                mesh_faces.push_back({v00, v20, v10});
                break;

                // verts 0, 1, 3 are inside
            case 0x0B:
                v00 = ev_map[e12];
                v10 = ev_map[e23];
                v20 = ev_map[e02];

                mesh_faces.push_back({v00, v20, v10});
                break;

                // verts 0, 2, 3 are inside
            case 0x0D:
                v00 = ev_map[e01];
                v10 = ev_map[e13];
                v20 = ev_map[e12];

                mesh_faces.push_back({v00, v20, v10});
                break;

                // verts 1, 2, 3 are inside
            case 0x0E:
                v00 = ev_map[e01];
                v10 = ev_map[e02];
                v20 = ev_map[e03];

                mesh_faces.push_back({v00, v20, v10});
                break;

            default:
                // assert(false);
                continue;
            }
        }

        std::ofstream outfile(filename); // create a file named "example.txt"

        if (outfile.is_open())
        {
            for (size_t k = 0; k < mesh_vertices.size(); ++k)
            {
                std::vector<double> site = mesh_vertices[k];
                size_t n = site.size();
                outfile << "v ";
                for (size_t i = 0; i < site.size(); i++)
                {
                    outfile << site[i] << " ";
                }
                outfile << " \n";
            }

            for (size_t j = 0; j < mesh_faces.size(); j++)
            {
                std::vector<int> vertices_idx = mesh_faces[j];

                outfile << "f " << vertices_idx[0] + 1 << " " << vertices_idx[1] + 1 << " " << vertices_idx[2] + 1 << " \n";
            }
        }
    }

    void MCMT::save_grid_mesh(std::string filename, float x_clip_plane)
    {

        std::vector<std::vector<double>> mesh_vertices;
        std::vector<std::vector<int>> mesh_faces;

        for (int i = 0; i < delaunay_->nb_vertices(); i++)
        {
            double x = delaunay_->vertex_ptr(i)[0];
            double y = delaunay_->vertex_ptr(i)[1];
            double z = delaunay_->vertex_ptr(i)[2];
            mesh_vertices.push_back(std::vector<double>{x, y, z});
        }

        for (int i = 0; i < delaunay_->nb_finite_cells(); i++)
        {
            std::vector<int> v_indices;
            bool flag = false;
            for (index_t lv = 0; lv < 4; ++lv)
            {
                int v = delaunay_->cell_vertex(i, lv);
                double x = delaunay_->vertex_ptr(v)[0];

                if (x < x_clip_plane)
                {
                    flag = true;
                    break;
                }
                v_indices.push_back(int(v));
            }
            if (flag)
                continue;
            mesh_faces.push_back(v_indices);
        }

        std::ofstream outfile(filename); // create a file named "example.txt"

        if (outfile.is_open())
        {
            for (size_t k = 0; k < mesh_vertices.size(); ++k)
            {
                std::vector<double> site = mesh_vertices[k];
                size_t n = site.size();
                outfile << "v ";
                for (size_t i = 0; i < site.size(); i++)
                {
                    outfile << site[i] << " ";
                }
                outfile << " \n";
            }

            for (size_t j = 0; j < mesh_faces.size(); j++)
            {
                std::vector<int> vertices_idx = mesh_faces[j];

                if (vertices_idx.size() == 4)
                {
                    outfile << "f " << vertices_idx[0] + 1 << " " << vertices_idx[2] + 1 << " " << vertices_idx[1] + 1 << " \n";
                    outfile << "f " << vertices_idx[0] + 1 << " " << vertices_idx[3] + 1 << " " << vertices_idx[2] + 1 << " \n";
                    outfile << "f " << vertices_idx[0] + 1 << " " << vertices_idx[1] + 1 << " " << vertices_idx[3] + 1 << " \n";
                    outfile << "f " << vertices_idx[1] + 1 << " " << vertices_idx[2] + 1 << " " << vertices_idx[3] + 1 << " \n";
                }
            }
        }
    }

}
