# ask libcheck to not fork each unit test when GDB is attached
set environment CK_FORK=no

# print vm_area at a node
define parean
	if $argc != 1
		printf "Use: parean <*area_node>\n"
	else
		p *((mc_vm_area *) ($arg0->data))
	end
end

# hex print vm_area at a node
define pxarean
	if $argc != 1
		printf "Use: parean <*area_node>\n"
	else
		p/x *((mc_vm_area *) ($arg0->data))
	end
end

# print vm_obj at a node
define pobjn
	if $argc != 1
		printf "Use: pobjn <*obj_node>\n"
	else
		p *((mc_vm_obj *) ($arg0->data))
	end
end

# hex print vm_obj at a node
define pxobjn
	if $argc != 1
		printf "Use: pobjn <*obj_node>\n"
	else
		p/x *((mc_vm_obj *) ($arg0->data))
	end
end

# print a node at the specified node
define pnode
	if $argc != 1
		printf "Use: pnode <*obj_area_node>\n"
	else
		p *(*((cm_lst_node **) ($arg0->data)))
	end
end

# print the area at a node at the specified node:
define pnodea
	if $argc != 1
		printf "Use: pnodea <*obj_area_node>\n"
	else
		p *((mc_vm_area *) ((*((cm_lst_node **) ($arg0->data)))->data))
	end
end

# hex print the area at a node at the specified node:
define pxnodea
	if $argc != 1
		printf "Use: pnodea <*obj_area_node>\n"
	else
		p/x *((mc_vm_area *) ((*((cm_lst_node **) ($arg0->data)))->data))
	end
end

# print the object at a node at the specified node:
define pnodeo
	if $argc != 1
		printf "Use: pnodea <*obj_area_node>\n"
	else
		p *((mc_vm_obj *) ((*((cm_lst_node **) ($arg0->data)))->data))
	end
end

# hex print the object at a node at the specified node:
define pxnodeo
	if $argc != 1
		printf "Use: pnodea <*obj_area_node>\n"
	else
		p/x *((mc_vm_obj *) ((*((cm_lst_node **) ($arg0->data)))->data))
	end
end

# print areas of a map
define pmapa
	if $argc != 1
		printf "Use: pmapa <*map>\n"
	else
		# print header
		printf " --- [AREAS] ---\n"
		
		# bootstrap iteration
		set $iter = 0
		set $iter_node = $arg0->vm_areas.head

		# for every area
		while $iter != $arg0->vm_areas.len

			# fetch & typecast next area
			set $area = ((mc_vm_area *) ($iter_node->data))

			# fetch relevant entries for this area
			set $id         = $area->id
			set $basename   = $area->basename
			set $start_addr = $area->start_addr
			set $end_addr   = $area->end_addr

			# fetch area's object id if one is present
			if $area->obj_node_p != 0
				set $obj_id = ((mc_vm_obj *) ($area->obj_node_p->data))->id
			else
				set $obj_id = -1337
			end

			# fetch area's last object i if one is present
			if $area->last_obj_node_p != 0
				set $last_obj_id = ((mc_vm_obj *) ($area->last_obj_node_p->data))->id
			else
				set $last_obj_id = -1337
			end

			# print relevant entries of this area
			printf "%-3d: 0x%-12lx - 0x%-12lx | obj: %-5d - last_obj: %-5d | \"%s\"\n", $id, $start_addr, $end_addr, $obj_id, $last_obj_id, $basename

			# advance iteration
			set $iter = $iter + 1
			set $iter_node = $iter_node->next
		end
	end 
end

# print unmapped areas of a map
define pmapua
	if $argc != 1
		printf "Use: pmapua <*map>\n"
	else
		# print header
		printf " --- [UNMAPPED AREAS] ---\n"
	
		# bootstrap iteration
		set $iter = 0
		set $iter_node = $arg0->vm_areas_unmapped.head

		# for every area
		while $iter != $arg0->vm_areas_unmapped.len

			# fetch & typecast next area
			set $area = ((mc_vm_area *) ((*((cm_lst_node **) ($iter_node->data)))->data))

			# fetch relevant entries for this area
			set $id         = $area->id
			set $basename   = $area->basename
			set $start_addr = $area->start_addr
			set $end_addr   = $area->end_addr

			# fetch area's object id if one is present
			if $area->obj_node_p != 0
				set $obj_id = ((mc_vm_obj *) ($area->obj_node_p->data))->id
			else
				set $obj_id = -1337
			end

			# fetch area's last object i if one is present
			if $area->last_obj_node_p != 0
				set $last_obj_id = ((mc_vm_obj *) ($area->last_obj_node_p->data))->id
			else
				set $last_obj_id = -1337
			end

			# print relevant entries of this area
			printf "%-3d: 0x%-12lx - 0x%-12lx | obj: %-5d - last_obj: %-5d | \"%s\"\n", $id, $start_addr, $end_addr, $obj_id, $last_obj_id, $basename

			# advance iteration
			set $iter = $iter + 1
			set $iter_node = $iter_node->next
		end
	end 
end

# print objs of a map
define pmapo
	if $argc != 1
		printf "Use: pmapo <*map>\n"
	else
		# print header
		printf " --- [OBJS] ---\n"
		
		# bootstrap iteration over objects
		set $iter = 0
		set $iter_node = $arg0->vm_objs.head

		# for every object
		while $iter != $arg0->vm_objs.len

			# fetch & typecast next object
			set $obj = ((mc_vm_obj *) ($iter_node->data))

			# fetch relevant entries of this object
			set $id         = $obj->id
			set $basename   = $obj->basename
			set $start_addr = $obj->start_addr
			set $end_addr   = $obj->end_addr

			# print relevant entries of this object
			printf "<%-3d: 0x%lx - 0x%lx | \"%s\">\n", $id, $start_addr, $end_addr, $basename

			# setup iteration over areas belonging to this object
			set $inner_iter = 0
			set $inner_iter_node = ((mc_vm_obj *) ($iter_node->data))->vm_area_node_ps.head

			# for every area that is part of this object
			printf "      [areas]:\n"
			while $inner_iter != ((mc_vm_obj *) ($iter_node->data))->vm_area_node_ps.len

				# fetch and typecast next area
				set $inner_area = ((mc_vm_area *) ((*((cm_lst_node **) ($inner_iter_node->data)))->data))

				# fetch relevant entries of this area
				set $id           = $inner_area->id
				set $basename     = $inner_area->basename
				set $start_addr   = $inner_area->start_addr
				set $end_addr     = $inner_area->end_addr

				#fetch area's object id if one is present
				if $inner_area->obj_node_p != 0
					set $inner_obj_id = ((mc_vm_obj *) ($inner_area->obj_node_p->data))->id
				else
					set $inner_obj_id = -1337
				end

				#fetch area's last object i if one is present
				if $inner_area->last_obj_node_p != 0
					set $inner_last_obj_id = ((mc_vm_obj *) ($inner_area->last_obj_node_p->data))->id
				else
					set $inner_last_obj_id = -1337
				end

				# print relevant entries of this area
				printf "        %-3d: 0x%-12lx - 0x%-12lx | obj: %-5d - last_obj: %-5d | \"%s\"\n", $id, $start_addr, $end_addr, $inner_obj_id, $inner_last_obj_id, $basename

				# advance iteration over areas
				set $inner_iter = $inner_iter + 1
				set $inner_iter_node = $inner_iter_node->next
			end

			# setup iteration over last areas belonging to this object
			set $inner_iter = 0
			set $inner_iter_node = ((mc_vm_obj *) ($iter_node->data))->last_vm_area_node_ps.head

			# for every area that is part of this object
			printf "      [last areas]:\n"
			while $inner_iter != ((mc_vm_obj *) ($iter_node->data))->last_vm_area_node_ps.len

				# fetch and typecast next area
				set $inner_area = ((mc_vm_area *) ((*((cm_lst_node **) ($inner_iter_node->data)))->data))

				# fetch relevant entries of this area
				set $id         = $inner_area->id
				set $basename   = $inner_area->basename
				set $start_addr = $inner_area->start_addr
				set $end_addr   = $inner_area->end_addr

				#fetch area's object id if one is present
				if $inner_area->obj_node_p != 0
					set $inner_obj_id = ((mc_vm_obj *) ($inner_area->obj_node_p->data))->id
				else
					set $inner_obj_id = -1337
				end

				#fetch last area's object i if one is present
				if $inner_area->last_obj_node_p != 0
					set $inner_last_obj_id = ((mc_vm_obj *) ($inner_area->last_obj_node_p->data))->id
				else
					set $inner_last_obj_id = -1337
				end

				# print relevant entries of this area
				printf "        %-3d: 0x%-12lx - 0x%-12lx | obj: %-5d - last_obj: %-5d | \"%s\"\n", $id, $start_addr, $end_addr, $inner_obj_id, $inner_last_obj_id, $basename

				# advance iteration over areas
				set $inner_iter = $inner_iter + 1
				set $inner_iter_node = $inner_iter_node->next
			end

			# advance iteration over objects
			set $iter = $iter + 1
			set $iter_node = $iter_node->next
		end
	end 
end

# print unmapped objs of a map
define pmapuo
	if $argc != 1
		printf "Use: pmapuo <*map>\n"
	else
		# print header
		printf " --- [UNMAPPED OBJS] ---\n"
		
		# bootstrap iteration over objects
		set $iter = 0
		set $iter_node = $arg0->vm_objs_unmapped.head

		# for every object
		while $iter != $arg0->vm_objs_unmapped.len

			# fetch & typecast next object
			set $obj = ((mc_vm_obj *) ((*((cm_lst_node **) ($iter_node->data)))->data))

			# fetch relevant entries of this object
			set $id            = $obj->id
			set $basename      = $obj->basename
			set $start_addr    = $obj->start_addr
			set $end_addr      = $obj->end_addr
			set $num_area      = $obj->vm_area_node_ps.len
			set $num_last_area = $obj->last_vm_area_node_ps.len

			# print relevant entries with conversion to MC_UNDEF_ADDR
			if ($start_addr == -1) && ($end_addr == -1)
				printf "<%-3d: MC_UNDEF_ADDR - MC_UNDEF_ADDR | areas: %d - last areas: %d | \"%s\">\n", $id, $num_area, $num_last_area, $basename
			else
				printf "<%-3d: 0x%-12lx - 0x%-12lx | areas: %d - last areas: %d | \"%s\">\n", $id, $start_addr, $end_addr, $num_area, $num_last_area, $basename
			end
			
			# advance iteration over objects
			set $iter = $iter + 1
			set $iter_node = $iter_node->next
		end
	end 
end


# session dependent (modify from here onwards)
tb main
run
layout src
