// History
//   2005/01/24 Shinigami: added getspyonclient2 to support packet 0xd9 (Spy on Client 2)
//   2005/08/29 Shinigami: character.spyonclient2 renamed to character.clientinfo
//                         getspyonclient2 renamed to getclientinfo
//   2007/07/09 Shinigami: added isUOKR [bool] - UO:KR client used?

#include "clib/stl_inc.h"

#include "clib/fdump.h"
#include "clib/logfile.h"
#include "clib/stlutil.h"
#include "clib/strutil.h" //CNXBUG
#include "clib/unicode.h"

#include "bscript/berror.h"

#include "account.h"
#include "cgdata.h"
#include "charactr.h"
#include "client.h"
#include "cliface.h"
#include "crypt/cryptengine.h"
#include "msgfiltr.h"
#include "polcfg.h"
#include "polsig.h"
#include "polstats.h"
#include "sockio.h"
#include "uniemod.h"
#include "uoclient.h"
#include "uvars.h"
#include "uworld.h"
#include "xbuffer.h"

// only in here temporarily, until logout-on-disconnect stuff is removed
#include "ufunc.h"

unsigned long Client::instance_counter_;

Client::Client( ClientInterface& aInterface, const string& encryption ) :
	acct(NULL),
	chr(NULL),
    Interface(aInterface),
	ready(false),
	csocket(-1),
    listen_port(0),
	disconnect(0),
	recv_state( RECV_STATE_CRYPTSEED_WAIT ),
    encrypted_data(),
    bufcheck1_AA(0xAA),
    buffer(),
    bufcheck2_55(0x55),
	bytes_received(0),
	message_length(0),
    cryptengine( create_crypt_engine( encryption ) ),
	encrypt_server_stream(0),
	msgtype_filter(&login_filter),
    fpLog(NULL),
	pause_count(0),
	first_xmit_buffer(NULL),
	last_xmit_buffer(NULL),
	n_queued(0),
    queued_bytes_counter(0),
    gd(new ClientGameData),
    instance_( ++instance_counter_ ),
    checkpoint(-1), //CNXBUG
    last_msgtype(255),
    thread_pid(-1),
    UOExpansionFlag(0),
	isUOKR(false),
    paused_(false)

{
    Interface.register_client( this );
	memset( buffer, 0, sizeof buffer );
    memset( &counters, 0, sizeof counters );
    memset( &clientinfo_, 0, sizeof(clientinfo_) );
}

void cancel_trade( Character* chr1 );

void Client::Delete( Client* client )
{
    client->PreDelete();
    delete client;
}

Client::~Client()
{
}

void Client::Disconnect()
{
    Interface.deregister_client( this );
	if (csocket != INVALID_SOCKET)//>= 0)
	{
#ifdef _WIN32
        shutdown( csocket,2 ); //2 is both sides, defined in winsock2.h ...
		closesocket( csocket );
#else
        shutdown( csocket,SHUT_RDWR );
		close( csocket );
#endif
	}
	csocket = INVALID_SOCKET;
}

void Client::PreDelete()
{
    Disconnect();
    // FIXME: TEMPORARY FIX: disassociate the character from the account, so
    // you can log on with another character
    if ((acct != NULL) &&
        (acct->active_character != NULL) &&
        (acct->active_character->client == this))
    {
        if (acct->active_character->logged_in)
        {
            Character* tchr = acct->active_character;
            ClrCharacterWorldPosition( tchr, "Client Exit" );
            send_remove_character_to_nearby( tchr );
            tchr->logged_in = false;

            tchr->set_opponent( NULL );
            tchr->removal_cleanup();
        }
        else
        {
            cerr << "Uhh...  active character not logged in!??" << endl;
        }
        acct->active_character = NULL;
    }

	// detach the account and character from this client, if they
	// are still associated with it.

	acct = NULL;

	if (chr)
    {
        if (chr->client == this)
            chr->client = NULL;
	    chr = NULL;
    }

    if (fpLog != NULL)
    {
        time_t now = time(NULL);
        fprintf( fpLog, "Log closed at %s\n", asctime( localtime( &now ) ) );

        fclose( fpLog );
        fpLog = NULL;
    }

    delete gd;
    gd = NULL;

    while (first_xmit_buffer != NULL)
    {
        XmitBuffer* xbuffer = first_xmit_buffer;
        first_xmit_buffer = first_xmit_buffer->next;
        free(xbuffer);
        --n_queued;
    }
    last_xmit_buffer = NULL;
}

// ClientInfo - delivers a lot of usefull infomation about client PC
BStruct* Client::getclientinfo() const
{
    BStruct* ret = new BStruct;

    ret->addMember( "unknown1",          new BLong( clientinfo_.unknown1 ) );          // Unknown - allways 0x02
    ret->addMember( "instance",          new BLong( clientinfo_.instance ) );          // Unique Instance ID of UO
    ret->addMember( "os_major",          new BLong( clientinfo_.os_major ) );          // OS Major
    ret->addMember( "os_minor",          new BLong( clientinfo_.os_minor ) );          // OS Minor
    ret->addMember( "os_revision",       new BLong( clientinfo_.os_revision ) );       // OS Revision
    ret->addMember( "cpu_manufacturer",  new BLong( clientinfo_.cpu_manufacturer ) );  // CPU Manufacturer
    ret->addMember( "cpu_family",        new BLong( clientinfo_.cpu_family ) );        // CPU Family
    ret->addMember( "cpu_model",         new BLong( clientinfo_.cpu_model ) );         // CPU Model
    ret->addMember( "cpu_clockspeed",    new BLong( clientinfo_.cpu_clockspeed ) );    // CPU Clock Speed [Mhz]
    ret->addMember( "cpu_quantity",      new BLong( clientinfo_.cpu_quantity ) );      // CPU Quantity
    ret->addMember( "memory",            new BLong( clientinfo_.memory ) );            // Memory [MB]
    ret->addMember( "screen_width",      new BLong( clientinfo_.screen_width ) );      // Screen Width
    ret->addMember( "screen_height",     new BLong( clientinfo_.screen_height ) );     // Screen Height
    ret->addMember( "screen_depth",      new BLong( clientinfo_.screen_depth ) );      // Screen Depth [Bit]
    ret->addMember( "directx_major",     new BLong( clientinfo_.directx_major ) );     // DirectX Major
    ret->addMember( "directx_minor",     new BLong( clientinfo_.directx_minor ) );     // DirectX Minor

    ObjArray* arr_vd;
    unsigned maxlen_vd = sizeof(clientinfo_.video_description) / sizeof(clientinfo_.video_description[0]);
    unsigned wlen_vd = 0;
    while ( (clientinfo_.video_description[wlen_vd] != L'\0') && (wlen_vd < maxlen_vd) )
      ++wlen_vd;
    if ( !convertUCtoArray(clientinfo_.video_description, arr_vd, wlen_vd, true) )
      ret->addMember( "video_description", new BError("Invalid Unicode speech received.") );
    else {
      ret->addMember( "video_description", arr_vd ); // Video Card Description [wide-character]
    }

    ret->addMember( "video_vendor",      new BLong( clientinfo_.video_vendor ) );      // Video Card Vendor ID
    ret->addMember( "video_device",      new BLong( clientinfo_.video_device ) );      // Video Card Device ID
    ret->addMember( "video_memory",      new BLong( clientinfo_.video_memory ) );      // Video Card Memory [MB]
    ret->addMember( "distribution",      new BLong( clientinfo_.distribution ) );      // Distribution
    ret->addMember( "clients_running",   new BLong( clientinfo_.clients_running ) );   // Clients Running
    ret->addMember( "clients_installed", new BLong( clientinfo_.clients_installed ) ); // Clients Installed
    ret->addMember( "partial_installed", new BLong( clientinfo_.partial_installed ) ); // Partial Insstalled

    ObjArray* arr_lc;
    unsigned maxlen_lc = sizeof(clientinfo_.langcode) / sizeof(clientinfo_.langcode[0]);
    unsigned wlen_lc = 0;
    while ( (clientinfo_.langcode[wlen_lc] != L'\0') && (wlen_lc < maxlen_lc) )
      ++wlen_lc;
    if ( !convertUCtoArray(clientinfo_.langcode, arr_lc, wlen_lc, true) )
      ret->addMember( "langcode", new BError("Invalid Unicode speech received.") );
    else {
      ret->addMember( "langcode", arr_lc ); // Language Code [wide-character]
    }

    ObjArray* arr_u2 = new ObjArray;
    for ( unsigned i = 0; i < sizeof(clientinfo_.unknown2); ++i )
      arr_u2->addElement( new BLong( clientinfo_.unknown2[i] ) );
    ret->addMember( "unknown2", arr_u2 ); // Unknown

    return ret;
}

std::string Client::status() const
{
    std::string st;
    if (acct != NULL)
        st += "AC:" + std::string(acct->name()) + " ";
    if (chr != NULL)
        st += "CH:" + chr->name() + " ";
    if (have_queued_data())
        st += "TXBUF ";
    if (disconnect)
        st += "DISC ";
    if (paused_)
        st += "PAUSE ";
    if (ready)
        st += "RDY ";
    st += ipaddrAsString() + " ";
    st += "CHK: " + decint(checkpoint) + " ";
    st += "PID: " + decint(thread_pid) + " ";
    st += "LAST: " + hexint(last_msgtype);
    return st;
}

void Client::setcryptseed( unsigned char i_cryptseed[4] )
{
    memcpy( cryptseed, i_cryptseed, sizeof cryptseed );
}

void Client::queue_data( const void *data, unsigned short datalen )
{
    THREAD_CHECKPOINT( active_client, 300 );
	XmitBuffer *xbuffer = (XmitBuffer *) malloc( sizeof(XmitBuffer)-1+datalen );
    THREAD_CHECKPOINT( active_client, 301 );
    if (xbuffer)
    {
        THREAD_CHECKPOINT( active_client, 302 );
	    xbuffer->next = NULL;
	    xbuffer->nsent = 0;
	    xbuffer->lenleft = datalen;
	    memcpy( xbuffer->data, data, datalen );
        THREAD_CHECKPOINT( active_client, 303 );
	    if (first_xmit_buffer == NULL)
	    {	// in this case, last_xmit_buffer is also NULL, so can't set its ->next.
            THREAD_CHECKPOINT( active_client, 304 );
		    first_xmit_buffer = xbuffer;
	    }
	    else
	    {
            THREAD_CHECKPOINT( active_client, 305 );
		    last_xmit_buffer->next = xbuffer;
	    }
        THREAD_CHECKPOINT( active_client, 306 );
	    last_xmit_buffer = xbuffer;
	    ++n_queued;
        queued_bytes_counter += datalen;
    }
    else
    {
        THREAD_CHECKPOINT( active_client, 307 );
        Log( "Client#%lu: Unable to allocate %d bytes for queued data.  Disconnecting.\n",
                instance_, sizeof(XmitBuffer)-1+datalen );
        disconnect = true;
    }
    THREAD_CHECKPOINT( active_client, 309 );

}


void Client::xmit( const void *data, unsigned short datalen )
{
    THREAD_CHECKPOINT( active_client, 200 );
	if (last_xmit_buffer) // this client already backlogged, schedule for later
	{
        THREAD_CHECKPOINT( active_client, 201 );
		queue_data( data, datalen );
        THREAD_CHECKPOINT( active_client, 202 );
        return;
	}
    THREAD_CHECKPOINT( active_client, 203 );

	/* client not backlogged - try to send. */
	const unsigned char *cdata = (const unsigned char *) data;
	int nsent;

	if (-1 == (nsent = send( csocket, (const char *)cdata, datalen, 0 )))
	{
        THREAD_CHECKPOINT( active_client, 204 );
        int sckerr = socket_errno;

        if (sckerr == SOCKET_ERRNO(EWOULDBLOCK))
		{
            THREAD_CHECKPOINT( active_client, 205 );
            Log( "Client#%lu: Switching to queued data mode (1, %u bytes)\n", instance_, (unsigned) datalen );
            cerr << "Switching to queued data mode (1, " << datalen << " bytes)" << endl;
            THREAD_CHECKPOINT( active_client, 206 );
			queue_data( data, datalen );
            THREAD_CHECKPOINT( active_client, 207 );
			return;
		}
		else
		{
            THREAD_CHECKPOINT( active_client, 208 );
            if (!disconnect)
                Log( "Client#%lu: Disconnecting client due to send() error (1): %d\n" , instance_, sckerr);
			disconnect = 1;
            THREAD_CHECKPOINT( active_client, 209 );
			return;
		}
	}
	else // no error
	{
        THREAD_CHECKPOINT( active_client, 210 );
		datalen -= nsent;
        counters.bytes_transmitted += nsent;
        polstats.bytes_sent += nsent;
		if (datalen)	// anything left? if so, queue for later.
		{
            THREAD_CHECKPOINT( active_client, 211 );
            Log( "Client#%lu: Switching to queued data mode (2)\n", instance_ );
            cerr << "Switching to queued data mode (2)" << endl;
            THREAD_CHECKPOINT( active_client, 212 );
			queue_data( cdata+nsent, datalen );
            THREAD_CHECKPOINT( active_client, 213 );

		}
	}
    THREAD_CHECKPOINT( active_client, 214 );
}

void Client::send_queued_data()
{
	XmitBuffer *xbuffer;
	// hand off data to the sockets layer until it won't take any more.
	// note if a buffer is sent in full, we try to send the next one, ad infinitum
	while (NULL != (xbuffer = first_xmit_buffer))
	{
		int nsent;
		nsent = send( csocket,
			          (char *) &xbuffer->data[xbuffer->nsent],
					  xbuffer->lenleft,
					  0 );
		if (nsent == -1)
		{
#ifdef _WIN32
            int sckerr = WSAGetLastError();
#else
            int sckerr = errno;
#endif
			if (sckerr == SOCKET_ERRNO(EWOULDBLOCK))
			{
				// do nothing.  it'll be re-queued later, when it won't block.
				return;
			}
			else
			{
                if (!disconnect)
                    Log( "Client#%lu: Disconnecting client due to send() error (2): %d\n", instance_, sckerr );
				disconnect = 1;
				return;
			}
		}
		else
		{
			xbuffer->nsent += nsent;
			xbuffer->lenleft -= nsent;
            counters.bytes_transmitted += nsent;
            polstats.bytes_sent += nsent;
			if (xbuffer->lenleft == 0)
			{
				first_xmit_buffer = first_xmit_buffer->next;
				if (first_xmit_buffer == NULL)
                {
					last_xmit_buffer = NULL;
                    Log( "Client#%lu: Leaving queued mode (%ld bytes xmitted)\n", instance_, queued_bytes_counter );
                    queued_bytes_counter = 0;
                }
				free( xbuffer );
				--n_queued;
			}
		}
	}
}

#define PRE_ENCRYPT

#ifndef PRE_ENCRYPT
#include "umsgtype.h"
#include "sockio.h"
#endif

// 33 01 "encrypted": 4F FA
static const unsigned char pause_pre_encrypted[2] = { 0x4F, 0xFA };
// 33 00 "encrypted": 4C D0
static const unsigned char restart_pre_encrypted[2] = { 0x4C, 0xD0 };

void Client::send_pause(bool bForce /*=false*/)
{
    if ((bForce || uoclient_protocol.EnableFlowControlPackets) && !paused_)
    {
#ifndef PRE_ENCRYPT
		MSG33_FLOW_CONTROL msg;
		msg.msgtype = MSGOUT_33_FLOW_CONTROL;
		msg.flow = MSGOPT_33_FLOW_PAUSE;
		transmit( this, &msg, sizeof msg );
#else
		xmit( pause_pre_encrypted, sizeof pause_pre_encrypted );
#endif
        paused_ = true;
        // cout << "Client#" << instance_ << " paused" << endl;
	}
}

void Client::pause()
{
	if (!pause_count)
	{
        send_pause();
        pause_count = 1;
	}
	// ++pause_count;
}

void Client::send_restart(bool bForce /*=false*/)
{
    if (paused_)
    {
#ifndef PRE_ENCRYPT
		MSG33_FLOW_CONTROL msg;
		msg.msgtype = MSGOUT_33_FLOW_CONTROL;
		msg.flow = MSGOPT_33_FLOW_RESTART;
		transmit( this, &msg, sizeof msg );
#else
		xmit( restart_pre_encrypted, sizeof restart_pre_encrypted );
#endif
        // cout << "Client#" << instance_ << " restarted" << endl;
        paused_ = false;
	}
}

void Client::restart()
{
/*
	--pause_count;

	if (!pause_count)
	{
        send_restart();
	}
*/
}

void Client::restart2()
{
    send_restart();
    pause_count = 0;
}
