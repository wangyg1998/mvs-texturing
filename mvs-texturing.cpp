#include <mve/mesh_io_ply.h>
#include <omp.h>
#include <util/file_system.h>
#include <util/system.h>
#include <util/timer.h>

#include <fstream>
#include <iostream>
#include <vector>

#include "TriMesh.h"
#include "XForm.h"
#include "tex/debug.h"
#include "tex/progress_counter.h"
#include "tex/settings.h"
#include "tex/texturing.h"
#include "tex/timer.h"
#include "tex/util.h"

int main()
{
	tex::Settings settings;
	//settings.keep_unseen_faces = true;

	Timer timer;
	util::WallTimer wtimer;

	std::cout << "Load and prepare mesh: " << std::endl;
	mve::TriangleMesh::Ptr mesh;
	mesh = mve::geom::load_ply_mesh("D:\\mvs-texturing\\data\\02\\post.ply");
	mve::MeshInfo mesh_info(mesh);
	tex::prepare_mesh(&mesh_info, mesh);

	std::cout << "Generating texture views: " << std::endl;
	tex::TextureViews texture_views;
	{
		trimesh::XForm<float> intrinsic;
		intrinsic.read("D:\\mvs-texturing\\data\\02\\leftIntrinsic.txt");
		for (int i = 0; i < 11; ++i)
		{
			trimesh::XForm<float> xf;
			xf.read("D:\\mvs-texturing\\data\\02\\" + std::to_string(i) + ".txt");
			trimesh::invert(xf);
			tex::TextureView view(i, xf.data(), intrinsic.data(), 1280, 1024, "D:\\mvs-texturing\\data\\02\\color_" + std::to_string(i) + ".png");
			texture_views.push_back(view);
		}
	}

	timer.measure("Loading");

	std::size_t const num_faces = mesh->get_faces().size() / 3;

	std::cout << "Building adjacency graph: " << std::endl;
	tex::Graph graph(num_faces);
	tex::build_adjacency_graph(mesh, mesh_info, &graph);

	if (true)
	{
		std::cout << "View selection:" << std::endl;
		util::WallTimer rwtimer;

		tex::DataCosts data_costs(num_faces, texture_views.size());
		if (true)
		{
			tex::calculate_data_costs(mesh, &texture_views, settings, &data_costs);
		}
		timer.measure("Calculating data costs");

		try
		{
			tex::view_selection(data_costs, &graph, settings);
		}
		catch (std::runtime_error& e)
		{
			std::cerr << "\tOptimization failed: " << e.what() << std::endl;
			std::exit(EXIT_FAILURE);
		}
		timer.measure("Running MRF optimization");
		std::cout << "\tTook: " << rwtimer.get_elapsed_sec() << "s" << std::endl;
	}

	tex::TextureAtlases texture_atlases;
	{
		/* Create texture patches and adjust them. */
		tex::TexturePatches texture_patches;
		tex::VertexProjectionInfos vertex_projection_infos;
		std::cout << "Generating texture patches:" << std::endl;
		tex::generate_texture_patches(graph, mesh, mesh_info, &texture_views, settings, &vertex_projection_infos, &texture_patches);

		if (true)
		{
			std::cout << "Running global seam leveling:" << std::endl;
			tex::global_seam_leveling(graph, mesh, mesh_info, vertex_projection_infos, &texture_patches);
			timer.measure("Running global seam leveling");
		}

		if (true)
		{
			std::cout << "Running local seam leveling:" << std::endl;
			tex::local_seam_leveling(graph, mesh, vertex_projection_infos, &texture_patches);
		}
		timer.measure("Running local seam leveling");

		/* Generate texture atlases. */
		std::cout << "Generating texture atlases:" << std::endl;
		tex::generate_texture_atlases(&texture_patches, settings, &texture_atlases);
	}

	/* Create and write out obj model. */
	{
		std::cout << "Building objmodel:" << std::endl;
		tex::Model model;
		tex::build_model(mesh, texture_atlases, &model);
		timer.measure("Building OBJ model");

		std::cout << "\tSaving model... " << std::flush;
		tex::Model::save(model, "D:\\output");
		std::cout << "done." << std::endl;
		timer.measure("Saving");
	}

	std::cout << "Whole texturing procedure took: " << wtimer.get_elapsed_sec() << "s" << std::endl;
	timer.measure("Total");

	if (true) //conf.write_view_selection_model
	{
		texture_atlases.clear();
		std::cout << "Generating debug texture patches:" << std::endl;
		{
			tex::TexturePatches texture_patches;
			generate_debug_embeddings(&texture_views);
			tex::VertexProjectionInfos vertex_projection_infos; // Will only be written
			tex::generate_texture_patches(graph, mesh, mesh_info, &texture_views, settings, &vertex_projection_infos, &texture_patches);
			tex::generate_texture_atlases(&texture_patches, settings, &texture_atlases);
		}

		std::cout << "Building debug objmodel:" << std::endl;
		{
			tex::Model model;
			tex::build_model(mesh, texture_atlases, &model);
			std::cout << "\tSaving model... " << std::flush;
			tex::Model::save(model, "D:\\debug");
			std::cout << "done." << std::endl;
		}
	}

	system("pause");
	return EXIT_SUCCESS;
}
