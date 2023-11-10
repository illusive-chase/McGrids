#include "fast_mcmt.hpp"
#include "sdfs.hpp"

int main(int argc, char **argv)
{
    using namespace GEO;
    double threshold = 1e-3;
    int constrain_res = 8;
    int num_constrain_points = constrain_res * constrain_res * constrain_res;

    std::vector<double> points;

    // always put constrained points in front
    for (int i = 0; i < constrain_res; i++)
    {
        for (int j = 0; j < constrain_res; j++)
        {
            for (int k = 0; k < constrain_res; k++)
            {
                points.push_back((1. / double(constrain_res - 1)) * i);
                points.push_back((1. / double(constrain_res - 1)) * j);
                points.push_back((1. / double(constrain_res - 1)) * k);
            }
        }
    }

    MCMT mcmt = MCMT();
    std::vector<double> point_values;
    for (int i = 0; i < points.size() / 3; i++)
    {
        point_values.push_back(SDF::sdBox(points[i * 3], points[i * 3 + 1], points[i * 3 + 2]));
    }
    mcmt.add_points(points.size() / 3, points.data(), point_values.data());
    
    for (int i=0; i<20; i++){
        point_values.clear();
        std::vector<double> sample_points = mcmt.sample_points(256);

       std::vector<double> relaxed_sample_points = mcmt.lloyd_relaxation(sample_points.data(), sample_points.size()/3, 5);
        for (int i = 0; i < sample_points.size() / 3; i++)
        {
            point_values.push_back(SDF::sdBox(relaxed_sample_points[i * 3], relaxed_sample_points[i * 3 + 1], relaxed_sample_points[i * 3 + 2]));
        }
        mcmt.add_points(relaxed_sample_points.size() / 3, relaxed_sample_points.data(), point_values.data());
        // std::cout << "Sampling iter: " << i << std::endl;
    }
    mcmt.output_grid_points("step_1_points.obj");
    mcmt.save_triangle_mesh("step_1_mesh_lloyd.obj");
    for (int i = 0; i < 100; i++)
    {
        point_values.clear();

        std::string filename = "grid_" + std::to_string(i) + ".obj";
        std::ofstream mid_file(filename);

        std::vector<double> mid_points = mcmt.get_mid_points();
        for (int j = 0; j < mid_points.size() / 3; j++)
        {
            point_values.push_back(SDF::sdBox(mid_points[j * 3], mid_points[j * 3 + 1], mid_points[j * 3 + 2]));
            mid_file << "v " << mid_points[j * 3] << " " << mid_points[j * 3 + 1] << " " << mid_points[j * 3 + 2] << std::endl;
        }
        std::vector<double> additional_points;
        std::vector<double> additional_point_values;

        for(int j=0; j<point_values.size(); j++){
            if(std::abs(point_values[j]) > threshold){
                additional_points.push_back(mid_points[j*3]);
                additional_points.push_back(mid_points[j*3+1]);
                additional_points.push_back(mid_points[j*3+2]);
                additional_point_values.push_back(point_values[j]);
            }
        }
        if (additional_points.size() == 0)
            break;
        mcmt.add_points(additional_points.size() / 3, additional_points.data(), additional_point_values.data());
        mcmt.save_triangle_mesh("step_2_mesh_" + std::to_string(i) + ".obj");
    }

    return 0;
}
