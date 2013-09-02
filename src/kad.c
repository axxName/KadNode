
#define _GNU_SOURCE

#include <netdb.h>
#include <net/if.h>
#include <sys/time.h>

#include "log.h"
#include "sha1.h"
#include "main.h"
#include "utils.h"
#include "conf.h"
#include "utils.h"
#include "results.h"
#include "net.h"
#include "dht_wrapper.c"


/*
The interface that is used to interact with the DHT.
*/

void kad_setup( void ) {
	int s4, s6;

	s4 = -1;
	s6 = -1;

	/* Let the DHT output debug text */
	if( gstate->verbosity == VERBOSITY_DEBUG ) {
		dht_debug = stdout;
	}

	dht_lock_init();

	if( gstate->af == AF_INET ) {
		s4 = net_bind( "DHT", DHT_ADDR4, gstate->dht_port, gstate->dht_ifce, IPPROTO_UDP, AF_INET );
		net_add_handler( s4, &dht_handler );
	} else {
		s6 = net_bind( "DHT", DHT_ADDR6, gstate->dht_port, gstate->dht_ifce, IPPROTO_UDP, AF_INET6 );
		net_add_handler( s6, &dht_handler );
	}

	/* Init the DHT.  Also set the sockets into non-blocking mode. */
	if( dht_init( s4, s6, gstate->node_id, (UCHAR*) "KN\0\0") < 0 ) {
		log_err( "DHT: Failed to initialize the DHT." );
	}
}

#ifdef DEBUG
void kad_debug_value_searches( int fd ) {
	char addrbuf[FULL_ADDSTRLEN+1];
	char hexbuf[HEX_LEN+1];
	struct result* vs;
	int i, j;

	j = 0;
	vs = results_list();
	while( vs != NULL ) {

		dprintf( fd, " Value Search: %s\n", str_id( vs->id, hexbuf ) );
		dprintf( fd, "  af: %s\n", (vs->af == AF_INET) ? "AF_INET" : "AF_INET6" );
		dprintf( fd, "  done: %d\n", vs->done );
		for( i = 0; i < vs->numaddrs; ++i ) {
			dprintf( fd, "   addr: %s\n", str_addr( &vs->addrs[i], addrbuf ) );
		}
		dprintf( fd, "  numaddrs: %zu\n", vs->numaddrs );

		j++;
		vs = vs->next;
	}
	dprintf( fd, " Found %d value searches.\n", j );
}

/* Print buckets (leaf/finger table) */
void kad_debug_buckets( int fd, struct bucket *b ) {
	char addrbuf[FULL_ADDSTRLEN+1];
	char hexbuf[HEX_LEN+1];
	struct node *n;
	int i, j;

	for( j = 0; b != NULL; ++j ) {
		dprintf( fd, " Bucket: %s\n", str_id( b->first, hexbuf ) );

		n = b->nodes;
		for( i = 0; n != NULL; ++i ) {
			dprintf( fd, "   Node: %s\n", str_id( n->id, hexbuf ) );
			dprintf( fd, "    addr: %s\n", str_addr( &n->ss, addrbuf ) );
			dprintf( fd, "    pinged: %d\n", n->pinged );
			n = n->next;
		}
		dprintf( fd, "  Found %d nodes.\n", i );
		b = b->next;
	}
	dprintf( fd, " Found %d buckets.\n", j );
}

/* Print searches */
void kad_debug_node_searches( int fd ) {
	char addrbuf[FULL_ADDSTRLEN+1];
	char hexbuf[HEX_LEN+1];
	struct search *s = searches;
	int i, j;

	for( j = 0; s != NULL; ++j ) {
		dprintf( fd, " Search: %s\n", str_id( s->id, hexbuf ) );
		dprintf( fd, "  af: %s\n", (s->af == AF_INET) ? "AF_INET" : "AF_INET6" );
		dprintf( fd, "  port: %hu\n", s->port );
		dprintf( fd, "  done: %d\n", s->done );
		for(i = 0; i < s->numnodes; ++i) {
			struct search_node *sn = &s->nodes[i];
			dprintf( fd, "   Node: %s\n", str_id(sn->id, hexbuf ) );
			dprintf( fd, "    addr: %s\n", str_addr( &sn->ss, addrbuf ) );
			dprintf( fd, "    pinged: %d\n", sn->pinged );
			dprintf( fd, "    replied: %d\n", sn->replied );
			dprintf( fd, "    acked: %d\n", sn->acked );
		}
		dprintf( fd, "  Found %d nodes.\n", i );
		s = s->next;
	}
	dprintf( fd, " Found %d node searches.\n", j );
}

/* Print announced ids we have received */
void kad_debug_storage( int fd ) {
	char addrbuf[FULL_ADDSTRLEN+1];
	char hexbuf[HEX_LEN+1];	
	struct storage *s = storage;
	IP addr;
	int i, j;

	for( j = 0; s != NULL; ++j ) {
		dprintf( fd, "Id: %s\n", str_id(s->id, hexbuf ));
		for( i = 0; i < s->numpeers; ++i ) {
			struct peer* p = &s->peers[i];
			if( p->len == 16 ) {
				IP6 *a = (IP6 *) &addr;
				a->sin6_family = AF_INET6;
				a->sin6_port = htons( p->port );
				memcpy( &a->sin6_addr, p->ip, 16 );
			} else {
				IP4 *a = (IP4 *) &addr;
				a->sin_family = AF_INET;
				a->sin_port = htons( p->port );
				memcpy( &a->sin_addr, p->ip, 4 );
			}
			dprintf( fd, "  Peer: %s\n", str_addr( &addr, addrbuf)  );
		}
		dprintf( fd, " Found %d peers.\n", i );
		s = s->next;
	}
	dprintf( fd, " Found %d stored hashes from received announcements.\n", j );
}

void kad_debug_blacklist( int fd ) {
	char addrbuf[FULL_ADDSTRLEN+1];
	int i;

	for( i = 0; i < (next_blacklisted % DHT_MAX_BLACKLISTED); i++ ) {
		dprintf( fd, " %s\n", str_addr( &blacklist[i], addrbuf) );
	}

	dprintf( fd, " Found %d blacklisted addresses.\n", i );
}

void kad_debug_announces( int fd ) {
	char hexbuf[HEX_LEN+1];
	struct value *v;

	v = gstate->values;
	while( v ) {
		dprintf( fd, " id: %s, port: %hu\n", str_id( v->value_id, hexbuf ), v->port );
		v = v->next;
	}
}

void kad_debug( int fd ) {

	dprintf( fd, "DHT_SEARCH_EXPIRE_TIME: %d\n", DHT_SEARCH_EXPIRE_TIME );
	dprintf( fd, "DHT_MAX_SEARCHES: %d\n", DHT_MAX_SEARCHES );

	/* maximum number of announced hashes we track */
	dprintf( fd, "DHT_MAX_HASHES: %d\n", DHT_MAX_HASHES );

	/* maximum number of peers for each announced hash we track */
	dprintf( fd, "DHT_MAX_PEERS: %d\n", DHT_MAX_PEERS );

	/* maximum number of blacklisted nodes */
	dprintf( fd, "DHT_MAX_BLACKLISTED: %d\n", DHT_MAX_BLACKLISTED );

	dht_lock();

	dprintf( fd, "\nAnnouncements:\n" );
	kad_debug_announces( fd );

	dprintf( fd, "\nBuckets:\n" );
	kad_debug_buckets( fd, (gstate->af == AF_INET) ? buckets : buckets6 );

	dprintf( fd, "\nNode Searches:\n" );
	kad_debug_node_searches( fd );

	dprintf( fd, "\nValue Searches:\n" );
	kad_debug_value_searches( fd );

	dprintf( fd, "\nStorage:\n" );
	kad_debug_storage( fd );

	dprintf( fd, "\nBlacklist:\n" );
	kad_debug_blacklist( fd );

	dht_unlock();
}
#endif

#define bprintf(...) (written += snprintf( buf+written, size-written, __VA_ARGS__))

int kad_status( char *buf, int size ) {
	char hexbuf[HEX_LEN+1];
	struct storage *strg = storage;
	struct search *srch = searches;
	struct value *value = gstate->values;
	int numsearches_active = 0;
	int numsearches_done = 0;
	int numstorage = 0;
	int numstorage_peers = 0;
	int numvalues = 0;
	int written = 0;

	/* count searches */
	while( srch != NULL ) {
		if( srch->done ) {
			numsearches_done++;
		} else {
			numsearches_active++;
		}
		srch = srch->next;
	}

	/* count storage and peers */
	while( strg != NULL ) {
		numstorage_peers += strg->numpeers;
		numstorage++;
		strg = strg->next;
	}

	while( value != NULL ) {
		numvalues++;
		value = value->next;
	}

	bprintf( "Node id: %s\n", str_id( myid, hexbuf ) );
	bprintf( "Bound to: %s:%s / %s\n",
		(gstate->af == AF_INET) ? "0.0.0.0" : "[::1]",
		gstate->dht_port,
		(gstate->dht_ifce == NULL) ? "<any>" : gstate->dht_ifce
	);

	if( gstate->af == AF_INET ) {
		bprintf( "Nodes: %d (IPv4)\n", count_nodes( buckets ) );
	} else {
		bprintf( "Nodes: %d (IPv6)\n", count_nodes( buckets6 ) );
	}

	bprintf( "Storage: %d (max %d), %d peers (max %d per storage)\n",
		numstorage, DHT_MAX_HASHES, numstorage_peers, DHT_MAX_PEERS );
	bprintf( "Searches: %d active, %d completed (max %d)\n",
		numsearches_active, numsearches_done, DHT_MAX_SEARCHES );
	bprintf( "Blacklist: %d (max %d)\n",
		(next_blacklisted % DHT_MAX_BLACKLISTED), DHT_MAX_BLACKLISTED );
	bprintf( "Values to announce: %d\n", numvalues );

	return written;
}


void kad_ping( const IP* addr ) {
	dht_lock();
	dht_ping_node( (struct sockaddr *)addr, addr_len(addr) );
	dht_unlock();
}

/*
* Find nodes that are near the given id and annouce
* that this node can satisfy the given id on the given port
*/
int kad_announce( const UCHAR *id, int port ) {

	if( port < 1 || port > 65535 ) {
		return 1;
	}

	dht_lock();
	dht_search( id, port, gstate->af, dht_callback_func, NULL );
	dht_unlock();

	return 0;
}

/*
* Start a search for nodes that are near the given id.
*/
int kad_search( const UCHAR *id ) {

	dht_lock();
	results_insert( id, gstate->af );
	dht_search( id, 0, gstate->af, dht_callback_func, NULL );
	dht_unlock();

	return 0;
}

/*
* Lookup known nodes that are nearest to the given id.
*/
int kad_lookup_value( const UCHAR* id, IP addr_array[], int *addr_num ) {
	int rc;
	struct result* vs;

	dht_lock();

	vs = results_find( id, gstate->af );

	if( vs == NULL ) {
		rc = 1;
	} else {
		*addr_num = MIN(vs->numaddrs, *addr_num);
		memcpy( addr_array, vs->addrs, *addr_num * sizeof(IP) );
		rc = (*addr_num == 0);
	}

	dht_unlock();

	return rc;
}

/*
* Lookup the address of the node that has the given id.
* The port refers to the kad instance.
*/
int kad_lookup_node( const UCHAR* id, IP *addr_return ) {
	struct search *sr;
	int i, rc;

	dht_lock();

	rc = 1;
	sr = searches;
    while( sr ) {
		if( sr->af == gstate->af && id_equal( sr->id, id ) ) {
			for( i = 0; i < sr->numnodes; ++i ) {
				if( id_equal( sr->nodes[i].id, id ) ) {
					memcpy( addr_return, &sr->nodes[i].ss, sizeof(IP) );
					rc = 0;
					goto done;
				}
			}
			break;
		}
        sr = sr->next;
    }

	done:;

	dht_unlock();

	return rc;
}

int kad_blacklist( const IP* addr ) {

	dht_lock();
	blacklist_node( NULL, (struct sockaddr *) addr, sizeof(IP) );
	dht_unlock();

	return 0;
}

int kad_export_nodes( IP addr_array[], int *num ) {
	IP4 addr4[32];
	IP6 addr6[32];
	int num4;
	int num6;
	int i, count;

	num6 = N_ELEMS( addr6 );
	num4 = N_ELEMS( addr4 );

	dht_lock();
	dht_get_nodes( addr4, &num4, addr6, &num6 );
	dht_unlock();

	count = 0;

	for( i = 0; i < num6 && count < *num; ++i, ++count ) {
		memcpy( addr_array, addr6, sizeof(IP6) );
	}

	for( i = 0; i < num4 && count < *num; ++i, ++count ) {
		memcpy( addr_array, addr4, sizeof(IP4) );
	}

	/* store number of nodes we have actually found */
	*num = count;

	return 0;
}