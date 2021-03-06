// History
// NOTE: In standingheight checks there is a nasty bug. Items NOT locked down
//       (unmovable), are NOT being checked correctly to block movement.

//   2005/06/01 Shinigami: added getmapshapes - to get access to mapshapes
//                         added getstatics - to fill a list with statics
//   2005/06/06 Shinigami: added readmultis derivative - to get a list of statics
//   2005/09/03 Shinigami: fixed: Realm::walkheight ignored param doors_block

#include "clib/stl_inc.h"

#include "realm.h"
#include "realmdescriptor.h"
#include "mapcell.h"
#include "mapserver.h"
#include "staticserver.h"
#include "mapshape.h"
#include "maptileserver.h"

#include "pol/tiles.h"
#include "pol/charactr.h"
#include "pol/cgdata.h"
#include "pol/customhouses.h"
#include "pol/fnsearch.h"
#include "pol/ssopt.h"
#include "pol/uvars.h"
#include "pol/uworld.h"
#include "pol/item.h"
#include "pol/itemdesc.h"
#include "pol/itemdesc.h"
#include "pol/multi.h"
#include "pol/house.h"
#include "pol/uconst.h"

#define HULL_HEIGHT_BUFFER 2

bool Realm::lowest_standheight( unsigned x, unsigned y, int* z ) const
{
    static MapShapeList vec;
    vec.clear();
    _mapserver->GetMapShapes( vec, x, y, FLAG::MOVELAND|FLAG::MOVESEA|FLAG::BLOCKING|FLAG::GRADUAL );

    bool res = true;
    standheight( MOVEMODE_LAND, vec, *z, &res, z );
    return res;
}

void Realm::standheight( MOVEMODE movemode,
                  MapShapeList& shapes, 
                  int oldz, 
                  bool* result_out, int* newz_out, int* gradual_boost ) const
{
    //int newz = *newz_inout;
    int lowest_blocking_z = 128;
    int highest_blocking_z = -127;
    bool land_ok = (movemode & MOVEMODE_LAND) ? true : false;
    bool sea_ok  = (movemode & MOVEMODE_SEA) ? true : false;
	int the_boost = 0;
	int new_boost = 0;

	if (gradual_boost != NULL)
		the_boost = *gradual_boost;
  
    if (shapes.size() == 1) //map only
    {
        const MapShape& shape = shapes.at(0);
        if ( land_ok && (shape.flags & (FLAG::MOVELAND)) )
        {
            *result_out = true;
            *newz_out = shape.z + shape.height;
            return;
        }
    }


//    int newz;
//    bool result = groundheight( x,y,&newz );
    int newz = -200;
    bool result = false;
    // bool air_ok  = (movemode & MOVEMODE_AIR) ? true : false;

    for( MapShapeList::const_iterator itr = shapes.begin(); itr != shapes.end(); ++itr )
    {
        const MapShape& shape = (*itr);
        unsigned long flags = shape.flags;
        int ztop = shape.z + shape.height;
#if ENABLE_POLTEST_OUTPUT
        if (static_debug_on)
        {
            cout << "static: graphic=0x" << std::hex << srec.graphic << std::dec 
                 << ", z=" << int(srec.z) 
                 << ", ht=" << int(srec.height) << endl;
        }
#endif

		if (the_boost < 5)
			the_boost = 5;


        if (shape.z < lowest_blocking_z)
            lowest_blocking_z = shape.z;
        if (ztop > highest_blocking_z)
            highest_blocking_z = ztop;

        if ( (land_ok && (flags&FLAG::MOVELAND)) ||
             (sea_ok && (flags&FLAG::MOVESEA)) )
        {
            if ( ((ztop <= oldz+2+the_boost) || ((flags&FLAG::GRADUAL)&&(ztop <= oldz+15)) ) &&   // not too high to step onto
                 (ztop >= newz) )     // but above or same as the highest yet seen
            {     
#if ENABLE_POLTEST_OUTPUT
                if (static_debug_on) cout << "Setting Z to " << int(ztemp) << endl;
#endif
                newz = ztop;
                result = true;
				if (flags&FLAG::GRADUAL)
					new_boost = shape.height;
            }
        }
        if ( newz < shape.z &&
                    shape.z < newz+PLAYER_CHARACTER_HEIGHT)  // space too small to stand?
        {

			result = false;
        }
    }
  

    if (result)
    {
        for( MapShapeList::const_iterator itr = shapes.begin(); itr != shapes.end(); ++itr )
        {
            const MapShape& shape = (*itr);
            if ((shape.flags & (FLAG::MOVELAND|FLAG::MOVESEA|FLAG::BLOCKING)) == 0)
                continue;
            int shape_top = shape.z + shape.height;

            if ( (newz < shape.z && 
						// FIXME: Must be left lower than 15 height like
						//        other checks, it will block char from walking down
						//        ladders in homes/dungeons if over 9.
					   shape.z <= oldz+9 /*PLAYER_CHARACTER_HEIGHT*/ && 
                       shape_top > oldz)
                    ||
                 (shape.z < newz+PLAYER_CHARACTER_HEIGHT && shape_top > newz) )

            {
#if ENABLE_POLTEST_OUTPUT
                if (static_debug_on)
                {
                    cout << "static: objtype=0x" << std::hex << srec.graphic << std::dec 
                         << ", z=" << int(srec.z) 
                         << ", ht=" << int(srec.height) 
                         << " blocks movement to z=" << int(newz) << endl;
                }
#endif

				result = false;
                break;
            }
        }
    }

    *result_out = result;
    *newz_out = newz;
	if (result && (gradual_boost != NULL))
	{
		if (new_boost > 11)
			*gradual_boost = 11;
		else
			*gradual_boost = new_boost;
	}
}


Item* find_walkon_item( Items& ivec, int z );
unsigned char flags_from_tileflags( unsigned long uoflags );

void Realm::readdynamics( MapShapeList& vec, unsigned x, unsigned y, Items& walkon_items, bool doors_block )
{
    ZoneItems& witems = getzone(x,y,this).items;
    for (ZoneItems::const_iterator itr = witems.begin(); itr != witems.end(); ++itr )
	{
		Item *item = *itr;
		if ((item->x == x) && (item->y == y))
        {
            if (tile_flags(item->graphic) & FLAG::WALKBLOCK)
		    { 
                if (doors_block || item->itemdesc().type != ItemDesc::DOORDESC )
                {
                    MapShape shape;
                    shape.z = item->z;
                    shape.height = item->height;
                    shape.flags = tile[ item->graphic ].flags;
                    vec.push_back( shape );
                }
            }
 
            if (has_walkon_script( item->objtype_ ))
            {
                walkon_items.push_back( item );
            }
		}
	}
}
UMulti* find_supporting_multi( MultiList& mvec, int z );

// new Z given new X, Y, and old Z.
bool Realm::walkheight(unsigned x, unsigned y, int oldz, 
                int* newz, 
                UMulti** pmulti, Item** pwalkon,
                bool doors_block,
                MOVEMODE movemode,
				int* gradual_boost)
{
    static MapShapeList shapes;
    static MultiList mvec;
    static Items walkon_items;
    
    if ( x >= _descriptor.width || 
         y >= _descriptor.height )
    {
        return false;
    }

    shapes.clear();
    mvec.clear();
    walkon_items.clear();
    
    readdynamics( shapes, x, y, walkon_items, doors_block /* true */ );
    readmultis( shapes, x, y, FLAG::MOVE_FLAGS, mvec );
    _mapserver->GetMapShapes( shapes, x, y, FLAG::MOVE_FLAGS );

    bool result;
    standheight(movemode, shapes, oldz, 
                &result, newz, gradual_boost);

    if (result)
    {
        if (!mvec.empty())
        {
            *pmulti = find_supporting_multi( mvec, *newz );
        }
        else
        {
            *pmulti = NULL;
        }

        if (!walkon_items.empty())
        {
            *pwalkon = find_walkon_item( walkon_items, *newz );
        }
        else
        {
            *pwalkon = NULL;
        }
        
    }

    return result;
}

// new Z given new X, Y, and old Z.
//dave: todo: return false if walking onto a custom house and not in the list of editing players, and no cmdlevel
bool Realm::walkheight( const Character* chr, unsigned x, unsigned y, int oldz, 
                int* newz, 
                UMulti** pmulti, Item** pwalkon, int* gradual_boost)
{
    static MapShapeList shapes;
    static MultiList mvec;
    static Items walkon_items;
    
    if ( x >= _descriptor.width || y >= _descriptor.height )
    {
        return false;
    }

    shapes.clear();
    mvec.clear();
    walkon_items.clear();
    
    readdynamics( shapes, x, y, walkon_items, chr->doors_block() );
    readmultis( shapes, x, y, FLAG::MOVE_FLAGS, mvec );
    _mapserver->GetMapShapes( shapes, x, y, FLAG::MOVE_FLAGS );

    bool result;
    standheight(chr->movemode, shapes, oldz, 
                &result, newz, gradual_boost);

    if (result)
    {
        if (!mvec.empty())
        {
            *pmulti = find_supporting_multi( mvec, *newz );
            if(*pmulti != NULL)
            {
                UHouse* house = (*pmulti)->as_house();
                if( house && house->editing && chr->client && (chr->client->gd->custom_house_serial != house->serial ) )
                    result = false; //sendsysmessage to client ?
            }
        }
        else
        {
            *pmulti = NULL;
            if(chr->client && chr->client->gd->custom_house_serial != 0)
            {
                UMulti* multi = system_find_multi(chr->client->gd->custom_house_serial);
                if(multi != NULL)
                {
                    UHouse* house = multi->as_house();
                    if(house != NULL)
                    {
                        CustomHouseStopEditing(const_cast<Character*>(chr), house);
                        CustomHousesSendFull(house, chr->client, HOUSE_DESIGN_CURRENT);
                    }
                }
            }
        }

        if (!walkon_items.empty())
        {
            *pwalkon = find_walkon_item( walkon_items, *newz );
        }
        else
        {
            *pwalkon = NULL;
        }
        
    }

    return result;
}

bool Realm::dropheight( unsigned dropx, 
                 unsigned dropy, 
                 int dropz,
                 int chrz, 
                 int* newz, 
                 UMulti** pmulti )
{
    static MapShapeList shapes;
    static MultiList mvec;
    static Items ivec;
    
    if ( dropx >= _descriptor.width || dropy >= _descriptor.height )
    {
        return false;
    }

    shapes.clear();
    mvec.clear();
    ivec.clear();
    
    readdynamics( shapes, dropx, dropy, ivec, true /* doors_block */ );
    readmultis( shapes, dropx, dropy, FLAG::DROP_FLAGS, mvec );
    _mapserver->GetMapShapes( shapes, dropx, dropy, FLAG::DROP_FLAGS );

    bool result = dropheight(shapes, dropz, chrz, newz);
    if (result)
    {
        if (!mvec.empty())
        {
            *pmulti = find_supporting_multi( mvec, *newz );
        }
        else
        {
            *pmulti = NULL;
        }
    }
    return result;
}

// used to be statics_dropheight
bool Realm::dropheight( MapShapeList& shapes, 
                 int dropz,
                 int chrz, 
                 int* newz ) const
{
    //sort( statics.begin(), statics.end() );

    int z = -128;
    bool result = false;

    for( MapShapeList::const_iterator itr = shapes.begin(); itr != shapes.end(); ++itr )
    {
        const MapShape& shape = (*itr);
        unsigned long flags = shape.flags;
        int ztop = shape.z + shape.height;
#if ENABLE_POLTEST_OUTPUT
        if (static_debug_on)
        {
            cout << "static: graphic=0x" << std::hex << srec.graphic << std::dec 
                 << ", z=" << int(srec.z) 
                 << ", ht=" << int(tileheight(srec.graphic)) << endl;
        }
#endif

        if (flags & FLAG::ALLOWDROPON)
        {
            if ( (ztop <= chrz+PLAYER_CHARACTER_HEIGHT) &&   // not too high to reach
                 (ztop >= z))          // but above or same as the highest yet seen
            {                           // NOTE, the >= here is so statics at ground height
                                        // will override a blocking map tile.
#if ENABLE_POLTEST_OUTPUT
                if (static_debug_on) cout << "Setting Z to " << int(ztemp) << endl;
#endif
                z=ztop;
                result = true;
            }
        }
    }

    *newz = z;
    
    if (result)
    {
        for( MapShapeList::const_iterator itr = shapes.begin(); itr != shapes.end(); ++itr )
        {
            const MapShape& shape = (*itr);
            if (shape.flags & FLAG::BLOCKING)
            {
                int ztop = shape.z + shape.height;

                if (shape.z <= z &&        // location blocked by some object
                               z < ztop)   
                {
    #if ENABLE_POLTEST_OUTPUT
                    if (static_debug_on)
                    {
                        cout << "static: objtype=0x" << std::hex << srec.graphic << std::dec 
                            << ", z=" << int(srec.z) 
                            << ", ht=" << int(tileheight(srec.graphic)) 
                            << " blocks movement to z=" << int(z) << endl;
                    }
    #endif
                    result = false;
                    break;
                }
                else if (z < ztop &&
                             ztop <= dropz)
                {
                    // a blocking item somewhere between where they _tried_ to drop it,
                    // and where we decided to place it.
                    result = false;
                    break;
                }
            }
        }
    }

    return result;

}

void Realm::readmultis( MapShapeList& vec, unsigned x, unsigned y, unsigned long anyflags ) const
{
    unsigned short wxL, wyL, wxH, wyH;
    zone_convert_clip( x-64, y-64, this, wxL, wyL );
    zone_convert_clip( x+64, y+64, this, wxH, wyH );

    for( unsigned short wx = wxL; wx <= wxH; ++wx )
    {
        for( unsigned short wy = wyL; wy <= wyH; ++wy )
        {
            ZoneMultis& wmultis = zone[wx][wy].multis;

            for( ZoneMultis::iterator itr = wmultis.begin(), end = wmultis.end(); itr != end; ++itr )
            {
                UMulti* multi = *itr;
                UHouse* house = multi->as_house();
                if( house != NULL && house->IsCustom() ) //readshapes switches to working design if the house is being edited, 
                                                         //everyone in the house would use it for walking...
                    multi->readshapes( vec, int(x) - multi->x, int(y) - multi->y, multi->z );
                else
                {
                    const MultiDef& def = multi->multidef();
                    def.readshapes( vec, int(x) - multi->x, int(y) - multi->y, multi->z, anyflags );
                }
            }
        }
    }
}

void Realm::readmultis( MapShapeList& vec, unsigned x, unsigned y, unsigned long anyflags, MultiList& mvec )
{
    unsigned short wxL, wyL, wxH, wyH;
    zone_convert_clip( x-64, y-64, this, wxL, wyL );
    zone_convert_clip( x+64, y+64, this, wxH, wyH );

    for( unsigned short wx = wxL; wx <= wxH; ++wx )
    {
        for( unsigned short wy = wyL; wy <= wyH; ++wy )
        {
            ZoneMultis& wmultis = zone[wx][wy].multis;

            for( ZoneMultis::iterator itr = wmultis.begin(), end = wmultis.end(); itr != end; ++itr )
            {
                UMulti* multi = *itr;
                UHouse* house = multi->as_house();
                if( house != NULL && house->IsCustom() )
                {
                    if( multi->readshapes( vec, int(x) - multi->x, int(y) - multi->y, multi->z ) )
                        mvec.push_back( multi );
                }
                else
                {
                    const MultiDef& def = multi->multidef();
                    if (def.readshapes( vec, int(x) - multi->x, int(y) - multi->y, multi->z, anyflags ))
                    {
                        mvec.push_back( multi );
                    }
                }
            }
        }
    }
}

void Realm::readmultis( StaticList& vec, unsigned x, unsigned y ) const
{
    unsigned short wxL, wyL, wxH, wyH;
    zone_convert_clip( x-64, y-64, this, wxL, wyL );
    zone_convert_clip( x+64, y+64, this, wxH, wyH );

    for( unsigned short wx = wxL; wx <= wxH; ++wx )
    {
        for( unsigned short wy = wyL; wy <= wyH; ++wy )
        {
            ZoneMultis& wmultis = zone[wx][wy].multis;

            for( ZoneMultis::iterator itr = wmultis.begin(), end = wmultis.end(); itr != end; ++itr )
            {
                UMulti* multi = *itr;
                UHouse* house = multi->as_house();
                if( house != NULL && house->IsCustom() ) //readshapes switches to working design if the house is being edited, 
                                                         //everyone in the house would use it for walking...
                    multi->readobjects( vec, int(x) - multi->x, int(y) - multi->y, multi->z );
                else
                {
                    const MultiDef& def = multi->multidef();
                    def.readobjects( vec, int(x) - multi->x, int(y) - multi->y, multi->z );
                }
            }
        }
    }
}

bool Realm::navigable( unsigned x, unsigned y, int z, int height = 0 ) const
{

	if (!valid( x,y,z ))
    {
        return false;
    }
	
	bool onwater = false;

    static MapShapeList shapes;
    shapes.clear();
    
    // possible: readdynamic, readmultis
    _mapserver->GetMapShapes( shapes, x, y, FLAG::ALL );

    for( MapShapeList::const_iterator itr = shapes.begin(); itr != shapes.end(); ++itr )
    {
        const MapShape& shape = (*itr);
        if (shape.flags & FLAG::MOVESEA)
        {
            onwater = true;
        }
        else
        {
			if ( ((z + height) >= (shape.z - HULL_HEIGHT_BUFFER)) &&
				 (z < (shape.z + shape.height + HULL_HEIGHT_BUFFER))
			   )
				return false;
        }
    }

    return onwater;
}

UMulti* Realm::find_supporting_multi( unsigned x, unsigned y, int z )
{
    if (!valid( x,y,z ))
    {
        return NULL;
    }

    static MapShapeList vec;
    static MultiList mvec;
    vec.clear();
    mvec.clear();
    readmultis( vec, x, y, FLAG::MOVE_FLAGS, mvec );

    return find_supporting_multi( mvec, z );
}

/* The supporting multi is the highest multi that is below or equal
 * to the Z-coord of the supported object.
 */
UMulti* Realm::find_supporting_multi( MultiList& mvec, int z )
{
    UMulti* found = NULL;
    for( MultiList::const_iterator itr = mvec.begin(), end = mvec.end(); itr != end; ++itr )
    {
        UMulti* multi = (*itr);
        if (multi->z <= z)
        {
            if ((found == NULL) ||
                (multi->z > found->z))
            {
                found = multi;
            }
        }
    }
    return found;
}

bool Realm::findstatic( unsigned x, unsigned y, unsigned objtype ) const
{
    return _staticserver->findstatic( x, y, objtype );
}

void Realm::getstatics( StaticEntryList& statics, unsigned x, unsigned y ) const
{
    _staticserver->getstatics( statics, x, y );
}

bool Realm::groundheight( unsigned x, unsigned y, int* z ) const
{
    MAPTILE_CELL cell = _maptileserver->GetMapTile( x, y );
    *z = cell.z;
    
    if (cell.landtile == GRAPHIC_NODRAW) // it's a nodraw tile
        *z = -128;
    
    return ( (cell.landtile<0x4000) && 
             ((landtile_flags(cell.landtile) & FLAG::BLOCKING) == 0) );
}

MAPTILE_CELL Realm::getmaptile( unsigned x, unsigned y ) const
{
    return _maptileserver->GetMapTile( x, y );
}

void Realm::getmapshapes( MapShapeList& shapes, unsigned x, unsigned y, unsigned long anyflags ) const
{
    _mapserver->GetMapShapes( shapes, x, y, anyflags );
}
