#include "BOX.H"

#include "CALCLARA.H"
#include "CONTROL.H"
#include "DELTAPAK.H"
#include "DRAW.H"
#include "ITEMS.H"
#include "LOT.H"

#include "SPECIFIC.H"
#include "TOMB4FX.H"

#ifdef PC_VERSION
#include "GAME.H"
#include "GLOBAL.H"
#else
#include "SETUP.H"
#include "MATHS.H"
#endif

#include "STYPES.H"
#include <assert.h>
#include "CAMERA.H"
#include "LARA.H"

#if PSX_VERSION || PSXPC_VERSION || SAT_VERSION
#include "MISC.H"
#include "SPHERES.H"
#endif


int number_boxes;
struct box_info* boxes;
unsigned short* overlap;
short* ground_zone[5][2];
unsigned short testclip;
unsigned short loops;

void DropBaddyPickups(struct ITEM_INFO* item)//259BC(<), 25BC8(<) (F)
{
	short pickup_number;
	short room_number;
	struct ITEM_INFO* pickup;

	for(pickup_number = item->carried_item; pickup_number != -1; pickup_number = pickup->carried_item)
	{
		pickup = &items[pickup_number];
		pickup->pos.x_pos = (item->pos.x_pos & 0xFFFFFC00) | 0x200;
		pickup->pos.z_pos = (item->pos.z_pos & 0xFFFFFC00) | 0x200;

		room_number = item->room_number;
		pickup->pos.y_pos = GetHeight(GetFloor(pickup->pos.x_pos, item->pos.y_pos, pickup->pos.z_pos, &room_number), 
			pickup->pos.x_pos, item->pos.y_pos, pickup->pos.z_pos);
		pickup->pos.y_pos -= GetBoundsAccurate(pickup)[3];

		ItemNewRoom(pickup_number, item->room_number);
		pickup->flags |= 0x20;
	}
}

int MoveCreature3DPos(struct PHD_3DPOS* srcpos, struct PHD_3DPOS* destpos, int velocity, short angdif, int angadd)// (F)
{
	int x = destpos->x_pos - srcpos->x_pos;
	int y = destpos->y_pos - srcpos->y_pos;
	int z = destpos->z_pos - srcpos->z_pos;
	int dist = phd_sqrt_asm(x * x + y * y + z * z);

	if (velocity < dist)
	{
		srcpos->x_pos += velocity * x / dist;
		srcpos->y_pos += velocity * y / dist;
		srcpos->z_pos += velocity * z / dist;
	}
	else
	{
		srcpos->x_pos = destpos->x_pos;
		srcpos->y_pos = destpos->y_pos;
		srcpos->z_pos = destpos->z_pos;
	}

	if (angdif <= angadd)
	{
		if (angdif >= -angadd)
			srcpos->y_rot = destpos->y_rot;
		else
			srcpos->y_rot -= angadd;
	}
	else
	{
		srcpos->y_rot += angadd;
	}

	return srcpos->x_pos == destpos->x_pos
		&& srcpos->y_pos == destpos->y_pos
		&& srcpos->z_pos == destpos->z_pos
		&& srcpos->y_rot == destpos->y_rot;
}

void CreatureYRot(struct PHD_3DPOS* srcpos, short angle, short angadd)//25738(<), ? (F)
{
	if (angadd < angle)
	{
		srcpos->y_rot += angadd;
		return;
	}//0x25768

	if (angle < -angadd)
	{
		srcpos->y_rot -= angadd;
		return;
	}//0x25788

	srcpos->y_rot += angle;

	return;
}

short SameZone(struct creature_info* creature, struct ITEM_INFO* target_item)//255F8(<), ? (F)
{
	struct room_info* r;
	short* zone;
	struct ITEM_INFO* item;

	item = &items[creature->item_num];

	zone = ground_zone[creature->LOT.zone][flip_status];

	r = &room[item->room_number];
	item->box_number = r->floor[((item->pos.z_pos - r->z) / 1024) + ((item->pos.x_pos - r->x) / 1024) * r->x_size].box;

	r = &room[target_item->room_number];
	target_item->box_number = r->floor[(target_item->pos.z_pos - r->z) / 1024 + ((target_item->pos.x_pos - r->x) / 1024) * r->x_size].box;
	
	return (zone[item->box_number] == zone[target_item->box_number]);
}

void FindAITargetObject(struct creature_info* creature, short obj_num)// (F)
{
	if (nAIObjects > 0)
	{
		struct ITEM_INFO* item = &items[creature->item_num];
		struct AIOBJECT* target_item = &AIObjects[0];
		short i;

		for (i = 0; i < nAIObjects; i++, target_item++)
		{
			if (target_item->object_number == obj_num
				&& target_item->trigger_flags == item->item_flags[3]
				&& target_item->room_number != 255)
			{
				short* zone = ground_zone[0][flip_status + 2 * creature->LOT.zone];
				struct room_info* r = &room[item->room_number];

				item->box_number = XZ_GET_SECTOR(r, item->pos.x_pos - r->x, item->pos.z_pos - r->z).box;
				target_item->box_number = XZ_GET_SECTOR(r, target_item->x - r->x, target_item->z - r->z).box;

				if (zone[item->box_number] == zone[target_item->box_number])
					break;
			}
		}

		creature->enemy = &creature->ai_target;

		creature->ai_target.object_number = target_item->object_number;
		creature->ai_target.room_number = target_item->room_number;
		creature->ai_target.pos.x_pos = target_item->x;
		creature->ai_target.pos.y_pos = target_item->y;
		creature->ai_target.pos.z_pos = target_item->z;
		creature->ai_target.pos.y_rot = target_item->y_rot;
		creature->ai_target.flags = target_item->flags;
		creature->ai_target.trigger_flags = target_item->trigger_flags;
		creature->ai_target.box_number = target_item->box_number;

		if (!(creature->ai_target.flags & 0x20))
		{
			creature->ai_target.pos.x_pos += CLICK * (SIN(creature->ai_target.pos.y_rot)) >> W2V_SHIFT;
			creature->ai_target.pos.z_pos += CLICK * (COS(creature->ai_target.pos.y_rot)) >> W2V_SHIFT;
		}
	}
}

void GetAITarget(struct creature_info* creature)
{
	UNIMPLEMENTED();
}

short AIGuard(struct creature_info* creature)//24DF0(<), ? (F)
{
	int random;

	if (items[creature->item_num].ai_bits & 5)
	{
		return 0;
	}

	random = GetRandomControl();

	if (random < 256)
	{
		creature->head_right = TRUE;
		creature->head_left = TRUE;
	}
	else if (random < 384)
	{
		creature->head_right = FALSE;
		creature->head_left = TRUE;
	}
	else if (random < 512)
	{
		creature->head_right = TRUE;
		creature->head_left = FALSE;
	}

	if (!creature->head_left)
		return (creature->head_right) << 12;

	if (creature->head_right)
		return 0;

	return -16384;
}

void AlertNearbyGuards(struct ITEM_INFO* item)//24D20(<), 24F2C(<) (F)
{
	int slot = 4;
	struct creature_info* cinfo = baddie_slots;
	struct ITEM_INFO* target;
	long x = 0;
	long y = 0;
	long z = 0;
	long distance = 0;
	int i = 0;

	//loc_24D3C
	for (i = 0; i < slot; i++)
	{
		if (cinfo->item_num == -1)
		{
			continue;
		}

		target = &items[cinfo->item_num + i];

		//Rooms match, alert the guards!
		if (item->room_number == target->room_number)
		{
			//24DCC
			cinfo->alerted = 1;
			continue;
		}

		x = (target->pos.x_pos - item->pos.x_pos) / 64;
		y = (target->pos.y_pos - item->pos.y_pos) / 64;
		z = (target->pos.z_pos - item->pos.z_pos) / 64;

		distance = (x * x) + (y * y) + (z * z);

		//Though the item is not in the same room.
		//If the distance between them is < 0x1F40, alert the guards!
		if (distance < 0x1F40)
		{
			//24DCC
			cinfo->alerted = 1;
		}
	}

	return;
}

void AlertAllGuards(short item_number)// (F)
{
	int slot;
	struct creature_info* cinfo = &baddie_slots[0];
	short obj_number = items[item_number].object_number;

	for(slot = 0; slot < 6; slot++, cinfo++)
	{
		struct ITEM_INFO* target;

		if (cinfo->item_num == -1)
		{
			continue;
		}

		target = &items[cinfo->item_num];

		if (obj_number == target->object_number)
		{
			if (target->status == ITEM_ACTIVE)
			{
				cinfo->alerted = TRUE;
			}
		}
	}
}

void CreatureKill(struct ITEM_INFO* item, int kill_anim, int kill_state, short lara_anim)
{
	UNIMPLEMENTED();
}

int CreatureVault(short item_number, short angle, int vault, int shift) // (F)
{
	struct ITEM_INFO* item = &items[item_number];
	long y = item->pos.y_pos;
	short room_number = item->room_number;

	long yy = item->pos.z_pos >> 10;
	long xx = item->pos.x_pos >> 10;

	CreatureAnimation(item_number, angle, 0);

	if (item->floor > y + 1152)
	{
		vault = 0;
	}
	else if (item->floor > y + 896)
	{
		vault = -4;
	}
	else if (item->floor > y + 640)
	{
		vault = -3;
	}
	else if (item->floor > y + 384)
	{
		vault = -2;
	}
	else if (item->pos.y_pos > y - 384)
	{
		return 0;
	}
	else if (item->pos.y_pos > y - 640)
	{
		vault = 2;
	}
	else if (item->pos.y_pos > y - 896)
	{
		vault = 3;
	}
	else if (item->pos.y_pos > y - 1152)
	{
		vault = 4;
	}

	if (yy == item->pos.z_pos >> 10)
	{
		if (xx > item->pos.x_pos >> 10)
		{
			item->pos.y_rot = ANGLE(-90);
			item->pos.x_pos = (xx << 10) + shift;
		}
		else if (xx < item->pos.x_pos >> 10)
		{
			item->pos.y_rot = ANGLE(90);
			item->pos.x_pos = (item->pos.x_pos << 10) - shift;
		}
		else
		{
			return 0;
		}
	}
	else if (xx == item->pos.x_pos >> 10)
	{
		if (yy > item->pos.z_pos >> 10)
		{
			item->pos.y_rot = ANGLE(-180);
			item->pos.z_pos = (yy << 10) + shift;
		}
		else
		{
			item->pos.y_rot = ANGLE(180);
			item->pos.z_pos = (item->pos.z_pos << 10) - shift;
		}
	}
	else
	{
		if (yy >= item->pos.z_pos >> 10)
		{
			item->pos.z_pos = (yy << 10) + shift;
		}
		else
		{
			item->pos.z_pos = (item->pos.z_pos << 10) - shift;
		}

		if (xx >= item->pos.x_pos >> 10)
		{
			item->pos.x_pos = (xx << 10) + shift;
		}
		else
		{
			item->pos.x_pos = (item->pos.x_pos << 10) - shift;
		}
	}

	item->floor = y;
	item->pos.y_pos = y;

	if (vault)
	{
		ItemNewRoom(item_number, room_number);
	}

	return vault;
}

short CreatureEffectT(struct ITEM_INFO* item, struct BITE_INFO* bite, short damage, short angle, short (*generate)(long x, long y, long z, short speed, short yrot, short room_number))// (F)
{
	struct PHD_VECTOR pos;

	pos.x = bite->x;
	pos.y = bite->y;
	pos.z = bite->z;

	GetJointAbsPosition(item, &pos, bite->mesh_num);

	return generate(pos.x, pos.y, pos.z, damage, angle, item->room_number);
}

short CreatureEffect(struct ITEM_INFO* item, struct BITE_INFO* bite, short(*generate)(long x, long y, long z, short speed, short yrot, short room_number))// (F)
{
	struct PHD_VECTOR pos;

	pos.x = bite->x;
	pos.y = bite->y;
	pos.z = bite->z;

	GetJointAbsPosition(item, &pos, bite->mesh_num);

	return generate(pos.x, pos.y, pos.z, item->speed, item->pos.y_rot, item->room_number);
}

void CreatureUnderwater(struct ITEM_INFO* item, long depth)//?, 2468C(<) (F)
{
	long water_level;
	long floorheight;
	short room_number;

	if (depth < 0)
	{
		water_level = -depth;
		depth = 0;
	}
	else
	{
		//0x246BC
		water_level = GetWaterHeight(item->pos.x_pos, item->pos.y_pos, item->pos.z_pos, item->room_number);
	}

	if (item->pos.y_pos < depth)
	{
		room_number = item->room_number;
		floorheight = GetHeight(GetFloor(item->pos.x_pos, item->pos.y_pos, item->pos.z_pos, &room_number), item->pos.x_pos, item->pos.y_pos, item->pos.z_pos);

		if (floorheight < depth)
		{
			item->pos.y_pos = floorheight;
		}
		else
		{
			//0x24724
			item->pos.y_pos = depth;
		}

		if (item->pos.x_rot > 0x16C)
		{
			item->pos.x_rot -= 0x16C;
		}
		else if (item->pos.x_rot > 0)
		{
			item->pos.x_rot = 0;
		}//0x24740
	}//0x2474c
}

void CreatureFloat(short item_number)//24524(<), (?) (F)
{
	long water_level;
	struct ITEM_INFO* item;
	short room_number;

	item = &items[item_number];
	item->hit_points = -16384;
	item->pos.x_rot = 0;
	water_level =  GetWaterHeight(item->pos.x_pos, item->pos.y_pos, item->pos.z_pos, item->room_number);

	if (water_level < item->pos.y_pos)
	{
		item->pos.y_pos -= 32;
	}//0x24590

	if (item->pos.y_pos < water_level)
	{
		item->pos.y_pos = water_level;
	}

	AnimateItem(item);
	room_number = item->room_number;

	item->floor = GetHeight(GetFloor(item->pos.x_pos, item->pos.y_pos, item->pos.z_pos, &room_number), item->pos.x_pos, item->pos.y_pos, item->pos.z_pos);

	if (room_number != item->room_number)
	{
		ItemNewRoom(item_number, room_number);
	}//0x245FC

	if (water_level < item->pos.y_pos)
	{
		return;
	}

	if (item->frame_number == anims[item->anim_number].frame_base)
	{
		item->meshswap_meshbits &= -7;
		item->active = 1;
		DisableBaddieAI(item_number);
		RemoveActiveItem(item_number);
	}

	item->after_death = 1;
}

void CreatureJoint(struct ITEM_INFO* item, short joint, short required)//24484(<) ? (F)
{
	short change;
	struct creature_info* crinfo;

	if (item->data == NULL)
	{
		return;
	}

	crinfo = (struct creature_info*)item->data;
	change = required - crinfo->joint_rotation[joint];

	if (change < ANGLE(-3))
	{
		change -= ANGLE(3);
	}
	else
	{
		change += ANGLE(3);
	}

	crinfo->joint_rotation[joint] += change;

	if (crinfo->joint_rotation[joint] < 0x3001)
	{
		crinfo->joint_rotation[joint] -= 0x3000;
	}
	else
	{
		crinfo->joint_rotation[joint] += 0x3000;
	}
}

void CreatureTilt(struct ITEM_INFO* item, short angle)//24418(<), 24624(<) (F)
{
	angle = (angle << 2) - item->pos.z_rot;

	if (angle < ANGLE(-3))
		angle = ANGLE(-3);
	else if (angle > ANGLE(3))
		angle = ANGLE(3);

	if (ABS(item->pos.z_rot) - ANGLE(15) > ANGLE(15))
	{
		angle >>= 1;
	}

	item->pos.z_rot += angle; // todo in orig code (mips) z_rot is lhu'd into v0 as unsigned, here i skipped that part, maybe it'll break
}

short CreatureTurn(struct ITEM_INFO* item, short maximum_turn)
{
	UNIMPLEMENTED();
	return 0;
}

int CreatureAnimation(short item_number, short angle, short tilt)
{
	UNIMPLEMENTED();
	return 0;
}

void CreatureDie(short item_number, int explode)// (F)
{
	struct ITEM_INFO* item = &items[item_number];
	item->hit_points = 0xC000;
	item->collidable = FALSE;
	if (explode)
	{
		if (objects[item->object_number].HitEffect == 1)
			ExplodingDeath2(item_number, -1, 258);
		else
			ExplodingDeath2(item_number, -1, 256);

		KillItem(item_number);
	}
	else
	{
		RemoveActiveItem(item_number);
	}

	DisableBaddieAI(item_number);
	item->flags |= IFLAG_KILLED | IFLAG_INVISIBLE;
	DropBaddyPickups(item);

	if (item->object_number == SCIENTIST && item->ai_bits == 20)
	{
		item = find_a_fucking_item(ROLLINGBALL);
		if (item)
		{
			if (!(item->flags & IFLAG_INVISIBLE))
			{
				item->flags |= IFLAG_ACTIVATION_MASK;
				AddActiveItem(item - items);
			}
		}
	}
}

int BadFloor(long x, long y, long z, long box_height, long next_height, int room_number, struct lot_info* LOT)// (F)
{
	short room_num = room_number;
	struct FLOOR_INFO* floor = GetFloor(x, y, z, &room_num);	
	long height;

	if (floor->box == 0x7FF)
		return TRUE;

	if (LOT->is_jumping)
		return FALSE;

	if (boxes[floor->box].overlap_index & LOT->block_mask)
		return TRUE;

	height = boxes[floor->box].height;

	if (box_height - height > LOT->step)
		return TRUE;

	if (box_height - height < LOT->drop)
		return TRUE;

	if (box_height - height < -LOT->step && height > next_height)
		return TRUE;

	if (LOT->fly && y > height + LOT->fly)
		return TRUE;

	return FALSE;
}

int CreatureCreature(short item_number) // (F)
{
	long x = items[item_number].pos.x_pos;
	long z = items[item_number].pos.z_pos;

	struct ITEM_INFO* item;
	short link;

	for(link = room[items[item_number].room_number].item_number; link != -1; link = item->next_item)
	{
		item = &items[link];

		if (link != item_number && item != lara_item && item->status == ITEM_ACTIVE && item->hit_points > 0)
		{
			long xdistance = ABS(item->pos.x_pos - x);
			long zdistance = ABS(item->pos.z_pos - z);
			long radius = xdistance <= zdistance ? zdistance + (xdistance >> 1) : xdistance + (zdistance >> 1);
			if (radius < objects[items[item_number].object_number].radius + objects[item->object_number].radius)
			{
				long y_rot = items[item_number].pos.y_rot;
				return phd_atan_asm(item->pos.z_pos - z, item->pos.x_pos - x) - y_rot;
			}
		}
	}

	return 0;
}

enum target_type CalculateTarget(struct PHD_VECTOR* target, struct ITEM_INFO* item, struct lot_info* LOT)
{
	UNIMPLEMENTED();
	return NO_TARGET;
}

void CreatureMood(struct ITEM_INFO* item, struct AI_info* info, int violent)
{
	UNIMPLEMENTED();
}

void GetCreatureMood(struct ITEM_INFO* item, struct AI_info* info, int violent)
{
	UNIMPLEMENTED();
}

int ValidBox(struct ITEM_INFO* item, short zone_number, short box_number)//222A4(<), ? (F)
{
	struct box_info* box;
	struct creature_info* creature;
	short* zone;

	creature = (struct creature_info*)item->data;
	zone = ground_zone[creature->LOT.zone][flip_status];

	if (creature->LOT.fly == 0 && zone[box_number] != zone_number)
	{
		return 0;
	}

	box = &boxes[box_number];

	if (box->overlap_index & creature->LOT.block_mask)
	{
		return 0;
	}

	if ((item->pos.z_pos > (box->left * 1024)) || ((box->right  * 1024) > item->pos.z_pos) ||
		(item->pos.x_pos > (box->top  * 1024)) || ((box->bottom * 1024) > item->pos.x_pos))
	{
		return 1;
	}

	return 0;
}

int EscapeBox(struct ITEM_INFO* item, struct ITEM_INFO* enemy, short box_number)//221C4(<), ?
{
	struct box_info *box; // $a0
	long x; // $a3
	long z; // $a2
	UNIMPLEMENTED();

	return 0;
}

void TargetBox(struct lot_info* LOT, short box_number)//220F4(<), ? (F)
{
	struct box_info* box;

	box_number &= 0x7FF;
	box = &boxes[box_number];

	LOT->required_box = box_number;

	LOT->target.z = (((((box->right - box->left) - 1) * GetRandomControl()) / 32) + (box->left * 1024)) + 512;
	LOT->target.x = (((((box->bottom - box->top) - 1) * GetRandomControl()) / 32) + (box->top * 1024)) + 512;

	if (LOT->fly == 0)
	{
		LOT->target.y = box->height;
	}
	else
	{
		LOT->target.y = box->height - 1152;
	}

	return;
}

int UpdateLOT(struct lot_info* LOT, int expansion)//22034(<), ? (F)
{
	struct box_node* expand;

	if (LOT->required_box != 0x7FF && LOT->required_box != LOT->target_box)
	{
		LOT->target_box = LOT->required_box;
		expand = &LOT->node[LOT->required_box];
		
		if (expand->next_expansion == 0x7FF && LOT->tail != LOT->required_box)
		{
			expand->next_expansion = LOT->head;

			if (LOT->head == LOT->node[LOT->required_box].next_expansion)
			{
				LOT->tail = LOT->target_box;
			}//0x220B8

			LOT->head = LOT->target_box;

		}//0x220C4

		expand->search_number = ++LOT->search_number;
		expand->exit_box = 0x7FF;
	}//220DC

	return SearchLOT(LOT, expansion);
}
//int /*$ra*/ SearchLOT(struct lot_info *LOT /*$t2*/, int expansion /*$s3*/)
int SearchLOT(struct lot_info* LOT, int expansion)
{
	int i; // $a2
	int index; // $v0
	int done; // $t8
	int change; // $a0
	int box_number; // $t1
	int overlap_flags; // $a2
	struct box_node* node; // $t5
	struct box_node* expand; // $a3
	struct box_info* box; // $t6
	short* zone; // $s0
	short search_zone; // $s2

	//v0 = flip_status << 2
	//a0 = &ground_zone
	//v1 = lot->zone << 3
	//v0 = &ground_zone[lot->zone][flip_status];
	//v1 = lot->head

	zone = ground_zone[LOT->zone][flip_status];
	search_zone = zone[LOT->head];
	
	//i = 0;

	if (expansion > 0)
	{
		//t7 = 0x7FF
		//s4 = overlap;

		//loc_21E40
		//v1 = LOT->head
		done = 0;
		if (LOT->head == 0x7FF)
		{
			LOT->tail = 0x7FF;
			return 0;
		}
		
		//loc_21E5C
		//s1 = a2 + 1;
		//v1 <<= 3;
		//v0 = LOT->node
		//a0 = boxes;
		node = &LOT->node[LOT->head];//t5
		box = &boxes[LOT->head];//t6
		//t3 = &overlap[(box->overlap_index & 0x3FFF)];
		//t9 = boxes;

		//loc_21E88
		box_number = overlap[(box->overlap_index & 0x3FFF)];
		//t3 += 2 (t3 is overlap[(box->overlap_index & 0x3FFF)];)
		overlap_flags = (box_number & 0x8000);

		if ((box_number & 0x8000))
		{
			done = 1;
		}
		
		//loc_21EA0
		//v0 = LOT->fly
		box_number &= 0x7FF;

		if (LOT->fly == 0 && search_zone != zone[box_number])
		{
			//loc_21FF4
		}
		//loc_21EC8
		//a1 = box_number << 3;
		//v0 = &boxes[box_number];
		//a0 = boxes[box_number].height 
		//v1 = box->height
		//v0 = LOT->step
		//t4 = boxes

		change = boxes[box_number].height - box->height;

		//v0 = overlap_flags & 0x2000
		if (LOT->step > change)
		{
			 //v0 = LOT->drop
			if (change < LOT->drop)
			{

			}//loc_21F20 = 0

		}
		//loc_21F04 fixme
		if (!(overlap_flags & 0x2000) || !LOT->can_monkey)
		{
			//loc_21FF4
		}

		if ((overlap_flags & 0x800) && !LOT->can_jump)
		{
			//loc_21FF4
		}
		//loc_21F40
		//v0 = LOT->node
		//a2 = node->search_number
		//a3 = LOT->node[box_number];
		//t0 = LOT->node[box_number].search_number;
		//a0 = (node->search_number & 0x7FFF)
		//v1 = (LOT->node[box_number].search_number & 0x7FFF)

		if ((node->search_number & 0x7FFF) < (LOT->node[box_number].search_number & 0x7FFF))
		{
			//loc_21FF4
		}

		if ((overlap_flags & 0x8000))
		{
			//loc_21FF4
		}
		
		if ((node->search_number & 0x7FFF) == (LOT->node[box_number].search_number & 0x7FFF))
		{
			//loc_21FF4
		}
		
		//loc_21F7C

	}//loc_22014
#if 0
				 sltu    $v0, $a0, $v1
				 bnez    $v0, loc_21FF4
				 andi    $v0, $a2, 0x8000
				 beqz    $v0, loc_21F7C
				 nop
				 beq     $a0, $v1, loc_21FF4
				 nop
				 j       loc_21FC0
				 sh      $a2, 2($a3)

				 loc_21F7C:
			 bne     $a0, $v1, loc_21F90
				 addu    $v0, $a1, $t4
				 andi    $v0, $t0, 0x8000
				 beqz    $v0, loc_21FF4
				 addu    $v0, $a1, $t4

				 loc_21F90 :
			 lh      $v1, 6($v0)
				 lhu     $a0, 0xA($t2)
				 nop
				 and     $v1, $a0
				 beqz    $v1, loc_21FB0
				 ori     $v0, $a2, 0x8000
				 j       loc_21FC0
				 sh      $v0, 2($a3)

				 loc_21FB0:
			 sh      $a2, 2($a3)
				 lhu     $v0, 4($t2)
				 nop
				 sh      $v0, 0($a3)

				 loc_21FC0 :
				 lh      $v0, 4($a3)
				 nop
				 bne     $v0, $t7, loc_21FF4
				 nop
				 lh      $v0, 6($t2)
				 nop
				 beq     $t1, $v0, loc_21FF4
				 sll     $v0, 3
				 lw      $v1, 0($t2)
				 nop
				 addu    $v0, $v1
				 sh      $t1, 4($v0)
				 sh      $t1, 6($t2)

				 loc_21FF4:
			 beqz    $t8, loc_21E88
				 move    $a2, $s1
				 lhu     $v0, 4($t5)
				 nop
				 sh      $v0, 4($t2)
				 slt     $v0, $a2, $s3
				 bnez    $v0, loc_21E40
				 sh      $t7, 4($t5)

				 loc_22014:
			 li      $v0, 1

				 loc_22018 :
				 lw      $s4, 0x18 + var_8($sp)
				 lw      $s3, 0x18 + var_C($sp)
				 lw      $s2, 0x18 + var_10($sp)
				 lw      $s1, 0x18 + var_14($sp)
				 lw      $s0, 0x18 + var_18($sp)
				 jr      $ra
				 addiu   $sp, 0x18
#endif
	return 0;
}

void CreatureAIInfo(struct ITEM_INFO* item, struct AI_info* info)
{
	UNIMPLEMENTED();
}

int CreatureActive(short item_number)//218B0(<), ? (F)
{
	struct ITEM_INFO* item = &items[item_number];

	if (item->flags & IFLAG_KILLED)
	{
		if (item->status != ITEM_INVISIBLE)
		{
			return 1;
		}

		if (EnableBaddieAI(item_number, 0) != 0)
		{
			item->status = ITEM_DEACTIVATED;
			return 1;
		}
	}

	return 0;
}

void InitialiseCreature(short item_number)//21800(<), ? (F)
{
	struct ITEM_INFO* item = &items[item_number];

	item->hit_status = 1;
	item->data = NULL;
	item->draw_room = (((item->pos.z_pos - room[item->room_number].z) / 1024) & 0xFF) | (((item->pos.x_pos - room[item->room_number].mesh->x) / 4) & 0xFF00);
	item->TOSSPAD = item->pos.y_rot & 0xE000;
	item->item_flags[2] = item->room_number | (item->pos.y_pos - room->minfloor);

	return;
}

int StalkBox(struct ITEM_INFO* item, struct ITEM_INFO* enemy, short box_number)//(F)
{
	struct box_info* box = &boxes[box_number];
	int baddie_quad;
	int box_quad;
	int enemy_quad;
	long x;
	long z;
	long xrange;
	long zrange;

	if (enemy == NULL)
	{
		return 0;
	}

	zrange = (box->right - box->left) << 10 + 3072;
	xrange = (box->bottom - box->top) << 10 + 3072;

	x = (box->top + box->bottom) << 9 - enemy->pos.x_pos;

	if (xrange < x || x < -xrange || zrange < z || z < -zrange)
		return 0;
	
	enemy_quad = (enemy->pos.y_rot << 14) + 2;

	if (z > 0)
	{
		if (x > 0)
		{
			box_quad = 2;
		}
		else
		{
			box_quad = 1;
		}
	}
	else
	{
		//loc_2176C
		if (x > 0)
		{
			box_quad = 3;
		}
		else
		{
			box_quad = 0;
		}
	}
	
	//loc_21778
	if (enemy_quad == box_quad)
		return 0;

	//loc_21788
	if (enemy->pos.z_pos < item->pos.z_pos)
	{
		if (enemy->pos.x_pos < item->pos.x_pos)
		{
			baddie_quad = 2;
		}
		else
		{
			baddie_quad = 1;
		}
	}
	else
	{
		//loc_217B8
		if (enemy->pos.x_pos < item->pos.x_pos)
		{
			baddie_quad = 3;
		}
		else
		{
			baddie_quad = 0;
		}
	}

	//loc_217D0
	if (enemy_quad != baddie_quad)
	{
		return 1;
	}

	if (ABS(enemy_quad - box_quad) == 2)
	{
		return 0;
	}

	return 1;
}

