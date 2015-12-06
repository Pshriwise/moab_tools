#include <iostream>
#include "cleanup.hpp"
//#include "MBAdaptiveKDTree.hpp"
#include "moab/OrientedBoxTreeTool.hpp"

namespace cleanup {
  // The obbtrees are no longer valid because the triangles have been altered.
  //  -Surface and volume sets are tagged with tags holding the obb tree
  //   root handles.
  //  -Surface/volume set handles are added to the root meshset.
  // Somehow, delete the old tree without deleting the
  // surface and volume sets, then build a new tree.
  moab::ErrorCode remove_obb_tree(bool verbose) {
    moab::ErrorCode result;
    moab::Range obb_entities;
    moab::Tag obbTag;
    result = MBI()->tag_get_handle( "OBB_TREE", sizeof(moab::EntityHandle),
	       		  moab::MB_TYPE_HANDLE, obbTag, moab::MB_TAG_DENSE, NULL, 0 );
    if(verbose)
    {
    if(gen::error(moab::MB_SUCCESS != result, "could not get OBB tree handle")) return result;
    }
    else
    {
    if(gen::error(moab::MB_SUCCESS != result, "")) return result;
    }
    // This gets the surface/volume sets. I don't want to delete the sets.
    // I want to remove the obbTag that contains the tree root handle and
    // delete the tree.
    result = MBI()->get_entities_by_type_and_tag( 0, moab::MBENTITYSET,
						    &obbTag, 0, 1, obb_entities );
    MB_CHK_SET_ERR(result, "");
    std::cout << "  found " << obb_entities.size() << " OBB entities" << std::endl;
    //gen::print_range( obb_entities );
    //result = MBI()->delete_entities( obb_entities );
 
    // find tree roots
    moab::Range trees;
    moab::OrientedBoxTreeTool tool( MBI() );
    moab::Tag rootTag;
    for(moab::Range::iterator i=obb_entities.begin(); i!=obb_entities.end(); i++) {
      moab::EntityHandle root;
      result = MBI()->tag_get_data( obbTag, &(*i), 1, &root );
      if(gen::error(moab::MB_SUCCESS!=result, "coule not get OBB tree data")) return result;
      //MB_CHK_SET_ERR(result, "");
      tool.delete_tree( root );
    }
    result = MBI()->tag_delete( obbTag ); // use this for DENSE tags
    MB_CHK_SET_ERR(result, "");


    result = MBI()->tag_get_handle ( "OBB", sizeof(double), 
     				  moab::MB_TYPE_DOUBLE, rootTag, moab::MB_TAG_SPARSE, 0, false);
    assert(moab::MB_SUCCESS==result || moab::MB_ALREADY_ALLOCATED==result);
    /*    result = MBI()->get_entities_by_type_and_tag( 0, moab::MBENTITYSET, &rootTag, 
                                                   NULL, 1, trees );
    if(moab::MB_SUCCESS != result) std::cout << "result=" << result << std::endl;
    MB_CHK_SET_ERR(result, "");
    //tool.find_all_trees( trees );
    std::cout << trees.size() << " tree(s) contained in file" << std::endl;
    //gen::print_range( trees );
  
    // delete the trees
    for (moab::Range::iterator i = trees.begin(); i != trees.end(); ++i) {
      result = tool.delete_tree( *i );
      //if(moab::MB_SUCCESS != result) std::cout << "result=" << result << std::endl;
      //MB_CHK_SET_ERR(result, "");
    }
    */ 
    // Were all of the trees deleted? Perhaps some of the roots we found were
    // child roots that got deleted with their parents.
    trees.clear();
    result = MBI()->get_entities_by_type_and_tag( 0, moab::MBENTITYSET, &rootTag,
						    NULL, 1, trees );
    MB_CHK_SET_ERR(result, "");
    std::cout << "  " << trees.size() << " OBB tree(s) contained in file" << std::endl;
    return moab::MB_SUCCESS;  
  }

  moab::ErrorCode delete_small_edge_and_tris( const moab::EntityHandle vert0, 
                                          moab::EntityHandle &vert1,
                                          const double tol ) {
    // If the verts are the same, this is not meaningful.
    if(vert0 == vert1) return moab::MB_SUCCESS;
    moab::ErrorCode result;

    // If the edge is small, delete it and the adjacent tris.
    if(tol > gen::dist_between_verts(vert0, vert1)) {
      // get tris to delete
      moab::Range tris;              
      moab::EntityHandle verts[2] = {vert0, vert1};                      
      result = MBI()->get_adjacencies( verts, 2, 2, false, tris );                   
      MB_CHK_SET_ERR(result, "");
      result = MBI()->delete_entities( tris );
      MB_CHK_SET_ERR(result, "");
      std::cout << "delete_small_edge_and_tris: deleted " << tris.size() 
                << " tris." << std::endl;
      // now merge the verts, keeping the first one
      // IN FUTURE, AVERAGE THE LOCATIONS???????????
      result = MBI()->merge_entities( vert0, vert1, false, true);
      MB_CHK_SET_ERR(result, "");
      vert1 = vert0;
    }
    return moab::MB_SUCCESS;
  }

  moab::ErrorCode delete_small_edges(const moab::Range &surfaces, const double FACET_TOL) {
    // PROBLEM: THIS IS INVALID BECAUSE TRIS CAN HAVE LONG EDGES BUT
    // SMALL AREA. All three pts are in a line. This is the nature of
    // faceting vs. meshing.
    /* Remove small triangles by removing edges that are too small. 
    Remove small edges by merging their endpoints together, creating
    degenerate triangles. Delete the degenerate triangles. */
    moab::ErrorCode result;
    for(moab::Range::const_iterator i=surfaces.begin(); i!=surfaces.end(); i++) {
      std::cout << "surf_id=" << gen::geom_id_by_handle(*i) << std::endl;

      // get all tris
      moab::Range tris;
      result = MBI()->get_entities_by_type( *i, moab::MBTRI, tris );
      MB_CHK_SET_ERR(result, "");

      // Check to ensure there area no degenerate tris
      for(moab::Range::iterator j=tris.begin(); j!=tris.end(); j++) {
        // get endpts
        const moab::EntityHandle *endpts;                                
        int n_verts;                                      
        result = MBI()->get_connectivity( *j, endpts, n_verts);
        MB_CHK_SET_ERR(result, "");
        assert(3 == n_verts);
        assert( endpts[0]!=endpts[1] && endpts[1]!=endpts[2] );
      }


      // get the skin first, because my find_skin does not check before creating edges.
      moab::Range skin_edges;
      //result = gen::find_skin( tris, 1, skin_edges, false );
      moab::Skinner tool(MBI());
      result = tool.find_skin( 0 , tris, 1, skin_edges, false );
      MB_CHK_SET_ERR(result, "");

      // create the edges
      moab::Range edges;
      result = MBI()->get_adjacencies( tris, 1, true, edges, moab::Interface::UNION );
      if(moab::MB_SUCCESS != result) {
	std::cout << "result=" << result << std::endl;
      }
      MB_CHK_SET_ERR(result, "");

      // get the internal edges
      moab::Range internal_edges = subtract(edges, skin_edges);

      for(moab::Range::iterator j=internal_edges.begin(); j!=internal_edges.end(); j++) {
        int n_internal_edges = internal_edges.size();
	std::cout << "edge=" << *j << std::endl;
        MBI()->list_entity( *j );
        MB_CHK_SET_ERR(result, "");

        // get endpts
        const moab::EntityHandle *endpts;                                
        int n_verts;                                      
        result = MBI()->get_connectivity( *j, endpts, n_verts);
        MB_CHK_SET_ERR(result, "");
        assert(2 == n_verts);

        // does another edge exist w the same endpts? Why would it?
        moab::Range duplicate_edges;
        result = MBI()->get_adjacencies( endpts, 2, 1, true, duplicate_edges );
        MB_CHK_SET_ERR(result, "");
        if(1 < duplicate_edges.size()) MBI()->list_entities( duplicate_edges );
        assert(1 == duplicate_edges.size());

        // if the edge length is less than MERGE_TOL do nothing
        if(FACET_TOL < gen::dist_between_verts( endpts[0], endpts[1] )) continue; 
 
        // quick check
        for(moab::Range::iterator k=internal_edges.begin(); k!=internal_edges.end(); k++) {
          const moab::EntityHandle *epts;                                
          int n_vts;                                      
          result = MBI()->get_connectivity( *k, epts, n_vts);
          MB_CHK_SET_ERR(result, "");
          assert(2 == n_vts);
	  // The skin edges/verts cannot be moved, therefore both endpoints cannot 
	  // be on the skin. If they are, continue.
	  moab::Range adj_edges0;
	  result = MBI()->get_adjacencies( &epts[0], 1, 1, true, adj_edges0 );
	  MB_CHK_SET_ERR(result, "");
	  if(3 > adj_edges0.size()) {
	    std::cout << "adj_edges0.size()=" << adj_edges0.size() 
		      << " epts[0]=" << epts[0] << std::endl;
	    MBI()->list_entity( epts[0] );
	    //MBI()->write_mesh( "test_output.h5m" );
	    MB_CHK_SET_ERR(result, "");
	  }
	  assert(3 <= adj_edges0.size());
	  moab::Range adj_skin_edges0 = intersect( adj_edges0, skin_edges );
	  bool endpt0_is_skin;
	  if(adj_skin_edges0.empty()) endpt0_is_skin = false;
	  else endpt0_is_skin = true;

	  moab::Range adj_edges1;
	  result = MBI()->get_adjacencies( &epts[1], 1, 1, true, adj_edges1 );
	  MB_CHK_SET_ERR(result, "");
	  if(3 > adj_edges1.size()) {
	    std::cout << "adj_edges1.size()=" << adj_edges1.size() 
		      << " epts[1]=" << epts[1] << std::endl;
	    MBI()->list_entity( epts[1] );
	    //MBI()->write_mesh( "test_output.h5m" );
	    MB_CHK_SET_ERR(result, "");
	  }
	  assert(3 <= adj_edges1.size());
        }


        // The skin edges/verts cannot be moved, therefore both endpoints cannot 
        // be on the skin. If they are, continue.
        moab::Range adj_edges0;
        result = MBI()->get_adjacencies( &endpts[0], 1, 1, true, adj_edges0 );
        MB_CHK_SET_ERR(result, "");
        if(3 > adj_edges0.size()) {
          std::cout << "adj_edges0.size()=" << adj_edges0.size() 
                    << " endpts[0]=" << endpts[0] << std::endl;
          MBI()->list_entity( endpts[0] );
          //MBI()->write_mesh( "test_output.h5m" );
          MB_CHK_SET_ERR(result, "");
        }
        assert(3 <= adj_edges0.size());
        moab::Range adj_skin_edges0 = intersect( adj_edges0, skin_edges );
        bool endpt0_is_skin;
        if(adj_skin_edges0.empty()) endpt0_is_skin = false;
        else endpt0_is_skin = true;

        moab::Range adj_edges1;
        result = MBI()->get_adjacencies( &endpts[1], 1, 1, true, adj_edges1 );
        MB_CHK_SET_ERR(result, "");
        if(3 > adj_edges1.size()) {
          std::cout << "adj_edges1.size()=" << adj_edges1.size() 
                    << " endpts[1]=" << endpts[1] << std::endl;
          MBI()->list_entity( endpts[1] );
          //MBI()->write_mesh( "test_output.h5m" );
          MB_CHK_SET_ERR(result, "");
        }
        assert(3 <= adj_edges1.size());
        moab::Range adj_skin_edges1 = intersect( adj_edges1, skin_edges );
        bool endpt1_is_skin;
        if(adj_skin_edges1.empty()) endpt1_is_skin = false;
        else endpt1_is_skin = true;
        if(endpt0_is_skin && endpt1_is_skin) continue;
        
        // Keep the skin endpt, and delete the other endpt
        moab::EntityHandle keep_endpt, delete_endpt;
        if(endpt0_is_skin) {
          keep_endpt   = endpts[0];
          delete_endpt = endpts[1];
        } else {
          keep_endpt   = endpts[1];
          delete_endpt = endpts[0];
        }

        // get the adjacent tris
	std::vector<moab::EntityHandle> adj_tris;
        result = MBI()->get_adjacencies( &(*j), 1, 2, false, adj_tris );
        MB_CHK_SET_ERR(result, "");
	        if(2 != adj_tris.size()) {
	std::cout << "adj_tris.size()=" << adj_tris.size() << std::endl;
	for(unsigned int i=0; i<adj_tris.size(); i++) gen::print_triangle( adj_tris[i], true );
        } 
        assert(2 == adj_tris.size());
 
        // When merging away an edge, a tri and 2 edges will be deleted.
        // Get each triangle's edge other edge the will be deleted.
        moab::Range tri0_delete_edge_verts;
        result = MBI()->get_adjacencies( &adj_tris[0], 1, 0, true, tri0_delete_edge_verts );
        MB_CHK_SET_ERR(result, "");
        assert(3 == tri0_delete_edge_verts.size());
        tri0_delete_edge_verts.erase( keep_endpt );
        moab::Range tri0_delete_edge;
        result = MBI()->get_adjacencies( tri0_delete_edge_verts, 1, true, tri0_delete_edge );
        MB_CHK_SET_ERR(result, "");
        assert(1 == tri0_delete_edge.size());      
 
        moab::Range tri1_delete_edge_verts;
        result = MBI()->get_adjacencies( &adj_tris[1], 1, 0, true, tri1_delete_edge_verts );
        MB_CHK_SET_ERR(result, "");
        assert(3 == tri1_delete_edge_verts.size());
        tri1_delete_edge_verts.erase( keep_endpt );
        moab::Range tri1_delete_edge;
        result = MBI()->get_adjacencies( tri1_delete_edge_verts, 1, true, tri1_delete_edge );
        MB_CHK_SET_ERR(result, "");
        assert(1 == tri1_delete_edge.size());      

        // When an edge is merged, it will be deleted and its to adjacent tris
        // will be deleted because they are degenerate. We cannot alter the skin.
        // How many skin edges does tri0 have?
	/*        moab::Range tri_edges;
        result = MBI()->get_adjacencies( &adj_tris[0], 1, 1, false, tri_edges );
        MB_CHK_SET_ERR(result, "");
        assert(3 == tri_edges.size());
        moab::Range tri0_internal_edges = intersect(tri_edges, internal_edges);

        // Cannot merge the edge away if the tri has more than one skin edge.
        // Otherwise we would delete a skin edge. We already know the edges in 
        // edge_set are not on the skin.
        if(2 > tri0_internal_edges.size()) continue;

        // check the other tri
        tri_edges.clear();
        result = MBI()->get_adjacencies( &adj_tris[1], 1, 1, false, tri_edges );
        MB_CHK_SET_ERR(result, "");
        assert(3 == tri_edges.size());
        moab::Range tri1_internal_edges = intersect(tri_edges, internal_edges);
        if(2 > tri1_internal_edges.size()) continue;

        // Check to make sure that the internal edges are not on the skin
        moab::Range temp;
        temp = intersect( tri0_internal_edges, skin_edges );
        assert(temp.empty());
        temp = intersect( tri1_internal_edges, skin_edges );
        assert(temp.empty());

        // We know that the edge will be merged away. Find the keep_vert and
        // delete_vert. The delete_vert should never be a skin vertex because the
        // skin must not move.
        moab::Range delete_vert;
        result = MBI()->get_adjacencies( tri0_internal_edges, 0, false, delete_vert);
        MB_CHK_SET_ERR(result, "");
        assert(1 == delete_vert.size());        
	*/

        // *********************************************************************
        // Test to see if the merge would create inverted tris. Several special
        // cases to avoid all result in inverted tris.
        // *********************************************************************
        // get all the tris adjacent to the point the will be moved.
        moab::Range altered_tris;
        result = MBI()->get_adjacencies( &delete_endpt, 1, 2, false, altered_tris );
        MB_CHK_SET_ERR(result, "");
        bool inverted_tri = false;
        for(moab::Range::const_iterator k=altered_tris.begin(); k!=altered_tris.end(); ++k) {
          const moab::EntityHandle *conn;
          int n_verts;
          result = MBI()->get_connectivity( *k, conn, n_verts );
          MB_CHK_SET_ERR(result, "");
          assert(3 == tris.size());
          moab::EntityHandle new_conn[3];
          for(unsigned int i=0; i<3; ++i) {
            new_conn[i] = (conn[i]==delete_endpt) ? keep_endpt : conn[i];
          }
          double area;
          result = gen::triangle_area( new_conn, area );
          MB_CHK_SET_ERR(result, "");
          if(0 > area) {
	    std::cout << "inverted tri detected, area=" << area << std::endl;
            inverted_tri = true;
            break;
          }
        }
        if(inverted_tri) continue;      

        // If we got here, then the merge will occur. Delete the edge the will be
        // made degenerate and the edge that will be made duplicate.
	std::cout << "A merge will occur" << std::endl;
      	gen::print_triangle( adj_tris[0], true );
	gen::print_triangle( adj_tris[1], true );
        //tri0_internal_edges.erase( *j );
        //tri1_internal_edges.erase( *j );
        internal_edges.erase( tri0_delete_edge.front() );
        internal_edges.erase( tri1_delete_edge.front() );
	std::cout << "merged verts=" << keep_endpt << " " << delete_endpt << std::endl;
	MBI()->list_entity( keep_endpt );
	MBI()->list_entity( delete_endpt );
        result = MBI()->merge_entities( keep_endpt, delete_endpt, false, true );          
	MB_CHK_SET_ERR(result, "");
        result = MBI()->delete_entities( tri0_delete_edge );
        MB_CHK_SET_ERR(result, "");
        result = MBI()->delete_entities( tri1_delete_edge );
        MB_CHK_SET_ERR(result, "");        
        result = MBI()->delete_entities( &(*j), 1 );
        MB_CHK_SET_ERR(result, "");
	std::cout << "deleted edges=" << *j << " " << tri0_delete_edge.front()
                  << " " << tri1_delete_edge.front() << std::endl;
	          
	// delete degenerate tris                          
	result = MBI()->delete_entities( &adj_tris[0], 2 );                 
	MB_CHK_SET_ERR(result, ""); 
	MBI()->list_entity( keep_endpt );

        // remove the edge from the range
        j = internal_edges.erase(*j) - 1; 
	std::cout << "next iter=" << *j << std::endl;        

        moab::Range new_tris;
        result = MBI()->get_entities_by_type( *i, moab::MBTRI, new_tris );
        MB_CHK_SET_ERR(result, "");
        moab::Range new_skin_edges;
        result = tool.find_skin( *i, new_tris, 1, new_skin_edges, false );
        MB_CHK_SET_ERR(result, "");
        assert(skin_edges.size() == new_skin_edges.size());
        for(unsigned int k=0; k<skin_edges.size(); k++) {
          if(skin_edges[k] != new_skin_edges[k]) {
            MBI()->list_entity( skin_edges[k] );
            MBI()->list_entity( new_skin_edges[k] );
          }
          assert(skin_edges[k] == new_skin_edges[k]);
        }
        assert(n_internal_edges = internal_edges.size()+3);
      }
  
      // cleanup edges
      result = MBI()->get_entities_by_type( 0, moab::MBEDGE, edges );
      MB_CHK_SET_ERR(result, "");
      result = MBI()->delete_entities( edges ); 
      MB_CHK_SET_ERR(result, ""); 
   
    }
    return moab::MB_SUCCESS;
  } 
  
  // Lots of edges have been created but are no longer needed.
  // Delete edges that are not in curves. These should be the only edges
  // that remain. This incredibly speeds up the watertight_check tool (100x?).
  moab::ErrorCode cleanup_edges( moab::Range curve_meshsets ) {
    moab::ErrorCode result;
    moab::Range edges, edges_to_keep;
    for(moab::Range::iterator i=curve_meshsets.begin(); i!=curve_meshsets.end(); i++) {
      result = MBI()->get_entities_by_dimension( *i, 1, edges );
      MB_CHK_SET_ERR(result, "");
      edges_to_keep.merge( edges );
    }

    moab::Range all_edges;
    result = MBI()->get_entities_by_dimension( 0, 1, all_edges );
    MB_CHK_SET_ERR(result, "");

    // delete the edges that are not in curves.
    //moab::Range edges_to_delete = all_edges.subtract( edges_to_keep );
    moab::Range edges_to_delete = subtract( all_edges, edges_to_keep );
    std::cout << "deleting " << edges_to_delete.size() << " unused edges" << std::endl;
    result = MBI()->delete_entities( edges_to_delete );
    MB_CHK_SET_ERR(result, "");
    return moab::MB_SUCCESS;
  }
 
  
}
