// History
//  2005/11/26 MuadDib: Added update code for updating highlight packet to client.
//  2005/11/26 Shinigami: added Character check
//	2005/11/25 MuadDib: Added distance check in handle_attack.
//  2006/03/07 MuadDib: Added Justice Region NoCombat check to attack request.

#include "clib/logfile.h"
#include "clib/stl_inc.h"
#include <stdio.h>

#include "clib/endian.h"

#include "account.h"
#include "client.h"
#include "charactr.h"
#include "msghandl.h"
#include "ufunc.h"
#include "umsgtype.h"

#include "cmbtcfg.h"
#include "guardrgn.h"
#include "cgdata.h"

void handle_attack( Client *client, MSG05_ATTACK *msg )
{
    if (client->chr->dead())
    {
        private_say_above( client->chr, client->chr, "I am dead and cannot do that." );
        return;
    }

	u32 serial = cfBEu32( msg->serial );
	Character *defender = find_character( serial );
	if (defender != NULL)
	{
		if ( !(combat_config.attack_self) )
		{
			if ( defender->serial == client->chr->serial )
			{
				client->chr->send_highlight();
				return;
			}
		}

		if (!client->chr->is_visible_to_me(defender)) {
			client->chr->send_highlight();
			return;
		}
		if (pol_distance(client->chr->x, client->chr->y, defender->x, defender->y) > 20) {
			client->chr->send_highlight();
			return;
		}

		if( defender->acct != NULL ) 
		{
			JusticeRegion* cur_justice_region = client->gd->justice_region;
			if( cur_justice_region->RunNoCombatCheck(defender->client) == true )
			{
				client->chr->send_highlight();
				send_sysmessage( client, "Combat is not allowed in this area." );
				return;
			}
		}
		client->chr->select_opponent( serial );
	}
	else
	{
		Character* attacker = client->chr;
        cerr << attacker->acct->name() << "/" << attacker->name() << " used Attack packet with illegal Defender serial." << endl;
        Log( "%s/%s used Attack packet with illegal Defender serial\n", attacker->acct->name(), attacker->name().c_str() );
	}
}
MESSAGE_HANDLER( MSG05_ATTACK, handle_attack );
