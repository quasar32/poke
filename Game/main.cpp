object *get_facing_npc(world &world, point test_pt)
    for(auto npc: world->maps.cur()->objects) {
        point npc_old_pos = npc->pos.to_quad_pt(); 
        if(npc->tick > 0) {
            point npc_dir_pt = g_next_pts[npc->dir];
            point npc_new_pt = npc_old_pt + npc_dir_pt;
            if(test_pt == npc_old_pt || test_pt == npc_new_pt) {
                new_quad_info.prop = quad_prop::solid; 
                return &npc;
            }
        } else if(test_pt == npc_old_point) {
            new_quad_info.prop = quad_prop::solid;
            if(!attempt_leap) {
                new_quad_info |= quad_prop::message;
            }
            return &npc;
        }
    }
    return nullptr;
}
