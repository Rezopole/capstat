
// {
//
//  capstat, renders a set of useful per-prefix, per-AS, etc, cross-repartitions. and more ....
//  Copyright (C) 2016 Jean-Daniel Pauget <jdpauget@rezopole.net>
//  
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//  
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//  
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// }



#include <iostream>
#include <iomanip>
#include <string>
#include <list>
#include <map>

#include <math.h>   // log2
#include <stdlib.h> // atol
#include <arpa/inet.h>	// inet_pton
#include <errno.h>  // errno
#include <string.h> // strerror

#include "macaddr.h"

#include "readline.h"


using namespace std;
using namespace stdjd;

size_t seek_ending_parenthesis (const string &s, size_t p) {
    size_t q = s.find ('(', p);
    if (q == string::npos) return p;

// cout << "HERE" << endl;
    p = q+1;
    int parenth_level = 1;
    do {
	q = s.find_first_of ("()", p);
// cout << "HERE 2" << endl;
	if (q == string::npos) return string::npos;
// cout << "HERE 3" << endl;
	if (s[q] == '(')
	    parenth_level ++;
	else if (s[q] == ')')
	    parenth_level --;
	else {
// cout << "HERE 4" << endl;
	    cerr << "seek_ending_parenthesis : what are we doing here ? [" << s.substr(p) << "]" << endl;
	    return string::npos;
	}
// cout << "HERE 5" << endl;
	p = q+1;
    } while (parenth_level > 0);
// cout << "HERE 6" << endl;
    return p;
}

// --------- Ethertype -------------------------------------------------------------------------------------------------------------------------

typedef enum {
    TETHER_IPV4,
    TETHER_IPV6,
    TETHER_ARP,
    TETHER_T8023,
    TETHER_MOPRC,
    TETHER_AOE,
    TETHER_UNKNOWN,
    TETHER_OTHER
} TEthertype;

class Ethertype {
  public:
    TEthertype ethertype;
    Ethertype () : ethertype (TETHER_UNKNOWN) {};
    Ethertype (const string &s) {
//cerr << "===================" << s.substr (0,20) << endl;
	     if (s.find ("ethertype IPv4 (0x0800)") == 0)      ethertype = TETHER_IPV4;
	else if (s.find ("ethertype IPv6 (0x86dd)") == 0)      ethertype = TETHER_IPV6;
	else if (s.find ("ethertype ARP (0x0806)") == 0)       ethertype = TETHER_ARP;
	else if (s.find ("802.3") == 0)                        ethertype = TETHER_T8023;
	else if (s.find ("ethertype MOP RC (0x6002)") == 0)    ethertype = TETHER_MOPRC;
	else if (s.find ("ethertype AoE (0x88a2)") == 0)       ethertype = TETHER_AOE;
	else if (s.find ("ethertype Unknown ") == 0)           ethertype = TETHER_UNKNOWN;
	else                                                   ethertype = TETHER_OTHER;
    }
    Ethertype (const Ethertype &o) : ethertype(o.ethertype) {}
    bool operator< (const Ethertype &a) const {
	return ethertype < a.ethertype;
    }
};
ostream &operator<< (ostream &out, const Ethertype &p) {
    switch (p.ethertype) {
	case TETHER_IPV4:    return out << "IPv4";
	case TETHER_IPV6:    return out << "IPv6";
	case TETHER_ARP:     return out << "ARP";
	case TETHER_T8023:   return out << "803.3";
	case TETHER_MOPRC:   return out << "MOP RC";
	case TETHER_AOE:     return out << "AoE";
	case TETHER_UNKNOWN: return out << "Unknown";
	case TETHER_OTHER:   return out << "Other";
	default:             return out << "Other";
    }
}

// --------- Level3Addr ------------------------------------------------------------------------------------------------------------------------

class Level3Addr;
ostream &operator<< (ostream &out, const Level3Addr &a);

class Level3Addr {
  public:
    TEthertype t;
    unsigned char b[16];

    bool valid (void) const {
	return ((t == TETHER_IPV4) || (t == TETHER_IPV6));
    }

    Level3Addr (void)  : t(TETHER_UNKNOWN) {
	size_t i;
	for (i=0 ; i<16 ; i++) b[i] = 0;
    }

    void applymask (Level3Addr const &mask) {
	size_t i;
	for (i=0 ; i<16 ; i++) b[i] &= mask.b[i];
    }

    Level3Addr (TEthertype proposed_type, string s) : t(TETHER_UNKNOWN) {
	size_t p, i;
	switch (proposed_type) {
	  case TETHER_IPV4:
	    b[0] = atoi (s.c_str());
	    p = s.find('.');
	    if (p!=string::npos) {
		b[1] = atoi (s.substr(p+1).c_str());
		p = s.find('.', p+1);
		if (p!=string::npos) {
		    b[2] = atoi (s.substr(p+1).c_str());
		    p = s.find('.', p+1);
		    if (p!=string::npos) {
			b[3] = atoi (s.substr(p+1).c_str());
			t = TETHER_IPV4;
		    }
		}
	    }
	    for (i=4 ; i<16 ; i++) b[i] = 0;	// we pad with zeros for comparisons
	    break;

	  case TETHER_IPV6:
	    p = s.find_first_not_of ("0123456789abcdefABCDEF:");
	    if (p == string::npos) {
		if (inet_pton(AF_INET6, s.c_str(), &(b[0])) == 1)
		    t = TETHER_IPV6;
	    } else {
		if (inet_pton(AF_INET6, s.substr(0,p).c_str(), &(b[0])) == 1)
		    t = TETHER_IPV6;
	    }
	    break;

	  default:
	    for (i=0 ; i<16 ; i++) b[i] = 0;
	    t = TETHER_UNKNOWN;
	}
//cout << "buit1 : " << *this << endl;
    }

    Level3Addr (Level3Addr const &r) : t(r.t) {
	size_t i;
	for (i=0 ; i<16 ; i++) b[i] = r.b[i];
//cout << "copy-buit : " << *this << " from " << r << endl;
    }
    bool operator< (const Level3Addr &a) const {
// cout << "comp   " << *this << " < " << a << "  --->   ";
	if (t < a.t) {
// cout << "yes" << endl;
	    return true;
	} else if (a.t < t) {
// cout << "no" << endl;
	    return false;
	} else {	// we have equal types
	    size_t i;
	    for (i=0 ; i<16 ; i++) {
		if (b[i] < a.b[i]) {
// cout << "yes" << endl;
		    return true;
		} else if (a.b[i] < b[i]) {
// cout << "no" << endl;
		    return false;
		}
	    }
	}
// cout << "NO (THERE)" << endl;
	return false;	// we should get there only if a == *this
    }

};
ostream &operator<< (ostream &out, const Level3Addr &a) {
//    Ethertype yolo;
//    yolo.ethertype = a.t;
//    out << "{" << yolo << "} ";
//    out << (a.valid() ? "_valid_ " : "_invalid_ ");
    switch (a.t) {
      case TETHER_IPV4:
	return out << (unsigned int)a.b[0] << '.' << (unsigned int)a.b[1] << '.' << (unsigned int)a.b[2] << '.' << (unsigned int)a.b[3];
	break;
      case TETHER_IPV6:
	char str[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &(a.b[0]), str, INET6_ADDRSTRLEN);
	return out << str;
	break;
      default:
	return out << "invalidL3addr";
	break;
    }
}

Level3Addr l3mask (int nb) {
    Level3Addr mask;
    mask.t = TETHER_IPV6;

    int bit = 0;

    for (bit=0 ; bit<128 ; bit++) {
	size_t i = bit / 8;
	size_t n = 7-(bit % 8);

	if (bit < nb) {	// we set one bit to 1
	    mask.b[i] |= (1 << n);
	} else {	// we set one bit to 0
	    mask.b[i] &= ~(1 << n);
	}
    }
    return mask;
}

// --------- Level3AddrPair : a pair of level-3 addresses, src / dst --------------------------------------------------------------------------------------------------------------

class Level3AddrPair {
  public:
    Level3Addr src, dst;
    Level3AddrPair () {}
    Level3AddrPair (Level3Addr src, Level3Addr dst) :
	src(src), dst(dst) {}
    Level3AddrPair (Level3AddrPair const & o) : src(o.src), dst(o.dst) {}
    bool operator< (const Level3AddrPair &a) const {
	if (a.src < src)
	    return true;
	else if (src < a.src)
	    return false;
	else if (a.dst < dst)
	    return true;
	else
	    return false;
    }
};
ostream &operator<< (ostream &out, const Level3AddrPair &p) {
    return out << "[" << p.src << " " << p.dst << "]";
}

// --------- PairMac : a pair of mac addresses, src / dst --------------------------------------------------------------------------------------------------------------------------

class PairMac {
  public:
    MacAddr src, dst;
    PairMac () {}
    PairMac (MacAddr src, MacAddr dst) :
	src(src), dst(dst) {}
    PairMac (PairMac const & o) : src(o.src), dst(o.dst) {}
    bool operator< (const PairMac &a) const {
	if (a.src < src)
	    return true;
	else if (src < a.src)
	    return false;
	else if (a.dst < dst)
	    return true;
	else
	    return false;
    }
};
ostream &operator<< (ostream &out, const PairMac &p) {
    return out << "[" << p.src << " " << p.dst << "]";
}

// --------- Qualifier -------------------------------------------------------------------------------------------------------------------------

class Qualifier {
  public:
    size_t nb;	    // number of packet, usually
    size_t len;	    // total length of said packets

    Qualifier (void) : nb(0), len(0) {};
    Qualifier (size_t nb, size_t len) : nb(nb), len(len) {};
    Qualifier (size_t len) : nb(1), len(len) {};
    Qualifier (const Qualifier &q) : nb(q.nb), len(q.len) {};
    Qualifier &operator+= (Qualifier const &q) {
	len += q.len;
	nb += q.nb;
	return *this;
    }
};

// --------- desc_[nb/len] Templates for map<T,Qualifier> types --------------------------------------------------------------------------------

template <typename T> bool desc_comparator_nb (typename map <T, Qualifier>::const_iterator mi1, typename map <T, Qualifier>::const_iterator mi2) {
    return mi1->second.nb > mi2->second.nb;
}


template <typename T> void dump_desc_nb (map <T, Qualifier> const &m, ostream &cout, Qualifier total, double ceil=1.0) {
    cout << m.size() << " entries" << ", total: " << total.nb << " packets" << endl;
    if ((m.size() ==0) || (total.nb==0))
	return;


    list <typename map <T, Qualifier>::const_iterator> l;
    typename map <T, Qualifier>::const_iterator mi;
    for (mi=m.begin() ; mi!=m.end() ; mi++)
	l.push_back (mi);

    l.sort (desc_comparator_nb<T>);

    int maxw = (int)(log2((*(l.begin()))->second.nb) / log2(10)) + 1;
    int nw = (int)(log2(l.size()) / log2(10)) + 1;
    size_t n = 0;
    size_t curtot = 0;
    typename list <typename map <T, Qualifier>::const_iterator>::const_iterator li;

    for (li=l.begin() ; li!=l.end() ; li++) {
	curtot += (*li)->second.nb;
	n++;
	cout << setw(nw)   << n << " "
	     << setw(10)   << (*li)->first << " "
	     << setw(maxw) << (*li)->second.nb << " "
	     << setw(3)    << (100*(*li)->second.nb)/total.nb << "% "
	     << setw(3)    << (100*curtot)/total.nb << "%"
	     << endl;
	if ((double)curtot/(double)total.nb > ceil) break;
    }
}

template <typename T> bool desc_comparator_len (typename map <T, Qualifier>::const_iterator mi1, typename map <T, Qualifier>::const_iterator mi2) {
    return mi1->second.len > mi2->second.len;
}


template <typename T> void dump_desc_len (map <T, Qualifier> const &m, ostream &cout, Qualifier total, double ceil=1.0) {
    cout << m.size() << " entries" << ", total: " << total.len << " bytes" << endl;
    if ((m.size() ==0) || (total.len==0))
	return;


    list <typename map <T, Qualifier>::const_iterator> l;
    typename map <T, Qualifier>::const_iterator mi;
    for (mi=m.begin() ; mi!=m.end() ; mi++)
	l.push_back (mi);

    l.sort (desc_comparator_len<T>);

    int maxw = (int)(log2((*(l.begin()))->second.len) / log2(10)) + 1;
    int nw = (int)(log2(l.size()) / log2(10)) + 1;
    size_t n = 0;
    size_t curtot = 0;
    typename list <typename map <T, Qualifier>::const_iterator>::const_iterator li;

    for (li=l.begin() ; li!=l.end() ; li++) {
	curtot += (*li)->second.len;
	n++;
	cout << setw(nw)   << n << " "
	     << setw(10)   << (*li)->first << " "
	     << setw(maxw) << (*li)->second.len << " "
	     << setw(3)    << (100*(*li)->second.len)/total.len << "% "
	     << setw(3)    << (100*curtot)/total.len << "%"
	     << endl;
	if ((double)curtot/(double)total.len > ceil) break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------

template <typename T> void insert_qualifier (map <T, Qualifier> &m, T const &key, Qualifier q) {
    typename map <T, Qualifier>::iterator mi = m.find (key);
    if (mi != m.end())
	mi->second += q;
    else
	m[key] = q;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------


map <MacAddr, Qualifier> rep_src_macaddr;
map <MacAddr, Qualifier> rep_dst_macaddr;
map <PairMac, Qualifier> rep_pair_macaddr;

map <Level3Addr, Qualifier> rep_l3src;
map <Level3Addr, Qualifier> rep_l3dst;
map <Level3AddrPair, Qualifier> rep_l3pair;

map <Ethertype, Qualifier> rep_ethertype;

size_t totsize = 0;
size_t nbpacket = 0;

int ipv4_mask = 24;
int ipv6_mask = ipv4_mask*2;

double percent_ceil = 0.9;

typedef enum {
    SEEK_NEXT_PACKET,
    SEEK_FIRST_IPV4,
} Treadtextstate;

int capstat (istream &cin, ostream &cout) {

    Level3Addr ipv4mask = l3mask (ipv4_mask);
    Level3Addr ipv6mask = l3mask (ipv6_mask);
cout << "ipv4mask = " << ipv4mask << endl;
cout << "ipv6mask = " << ipv6mask << endl << endl ;
    size_t lno = 0;
    Treadtextstate state = SEEK_NEXT_PACKET;
    while (cin) {
	MacAddr src, dst;
	Level3Addr l3src, l3dst;
	Ethertype ethertype;
	long packetlen = 0;

	do {
	    string s;
	    lno++; readline (cin, s);
	    size_t p;

	    switch (state) {
	      case SEEK_NEXT_PACKET:
		if (isdigit(s[0])) {    // the line must start with a time-stamp
		    p = s.find(' ');
		    if (p != string::npos) {
			src = MacAddr (s.substr(p+1));
			if (src.invalid()) cerr << "   ligne : " << lno << endl;
			p = s.find (" > ", p+1);
			if (p != string::npos) {
			    dst = MacAddr (s.substr(p+3));
			    if (dst.invalid()) cerr << "   ligne : " << lno << endl;
			    p = s.find (", ", p+3);
			    if (p != string::npos) {
				ethertype = Ethertype (s.substr (p+2));
				p = s.find (", length ", p+2);
				if (p != string::npos) {
				    packetlen = atol (s.substr(p+9, 10).c_str());
				    if (ethertype.ethertype == TETHER_IPV6) {
					p = seek_ending_parenthesis (s, p+9);
					l3src = Level3Addr(TETHER_IPV6, s.substr (p+1));
					l3src.applymask (ipv6mask);
					p = s.find (" > ", p);
					if (p != string::npos) {
					    l3dst = Level3Addr(TETHER_IPV6, s.substr (p+3));
					    l3dst.applymask (ipv6mask);
					}
				    }
				}
			    }
			}
		    }
		}
		if (ethertype.ethertype == TETHER_IPV4)
		    state = SEEK_FIRST_IPV4;
		break;

	      case SEEK_FIRST_IPV4:
		if (isdigit(s[0])) {
		    state = SEEK_NEXT_PACKET;
		    break;
		}
		p = s.find_first_not_of (' ');
		if (p != string::npos) {
		    l3src = Level3Addr(TETHER_IPV4, s.substr (p));
		    l3src.applymask (ipv4mask);
////	if (l3src.valid()) {
////	cout << "omask = " << ipv4mask << endl << endl ;
////	cout << "from : " << s << endl
////	   << "got  : " << l3src << endl
////	   << endl;
////	//cout << "----> IPv4 " << s.substr (p, s.find (' ', p) - p) << endl;
////	}
		    p = s.find (" > ", p);
		    if (p != string::npos) {
			l3dst = Level3Addr(TETHER_IPV4, s.substr (p+3));
			l3dst.applymask (ipv4mask);
////	if (l3dst.valid()) {
////	cout << "omask = " << ipv4mask << endl << endl ;
////	cout << "from : " << s << endl
////	   << "got  : " << l3dst << endl
////	   << endl;
////	//cout << "----> IPv4 " << s.substr (p, s.find (' ', p) - p) << endl;
////	}
		    }
		}

		state = SEEK_NEXT_PACKET;
		break;
	    }
	} while (cin && (state != SEEK_NEXT_PACKET));

	if (src.valid()) {
	    nbpacket ++;
	    totsize += packetlen;

//template <typename T> void insert_qualifier (map <T, Qualifier> &m, T const &key, Qualifier q) {
	    Qualifier q (packetlen);
	    insert_qualifier (rep_src_macaddr, src, q);
	    if (dst.valid()) {
		insert_qualifier (rep_dst_macaddr, dst, q);
		PairMac pair(src, dst);
		insert_qualifier (rep_pair_macaddr, pair, q);
		insert_qualifier (rep_ethertype, ethertype, q);
	    }
	    if (l3src.valid()) {
		insert_qualifier (rep_l3src, l3src, q);
		if (l3dst.valid()) {
		    insert_qualifier (rep_l3dst, l3dst, q);
		    Level3AddrPair pair(l3src, l3dst);
		    insert_qualifier (rep_l3pair, pair, q);
		}
	    }
		

//	    map <MacAddr, Qualifier>::iterator mi = rep_src_macaddr.find(src);
//	    if (mi != rep_src_macaddr.end())
//		mi->second += Qualifier (packetlen);
//	    else
//		rep_src_macaddr[src] = Qualifier (packetlen);
//
//	    if (dst.valid()) {
//		map <MacAddr, Qualifier>::iterator mi = rep_dst_macaddr.find(dst);
//		if (mi != rep_dst_macaddr.end())
//		    mi->second += Qualifier (packetlen);
//		else
//		    rep_dst_macaddr[dst] = Qualifier (packetlen);
//
//		PairMac pair(src, dst);
//		{
//		map <PairMac, Qualifier>::iterator mi = rep_pair_macaddr.find(pair);
//		if (mi != rep_pair_macaddr.end())
//		    mi->second += Qualifier (packetlen);
//		else
//		    rep_pair_macaddr[pair] = Qualifier (packetlen);
//		}
//		
//	    }
//	    {	map <Ethertype, Qualifier>::iterator mi = rep_ethertype.find (ethertype);
//		if (mi != rep_ethertype.end())
//		    mi->second += Qualifier (packetlen);
//		else
//		    rep_ethertype[ethertype] = Qualifier (packetlen);
//	    }
//	    if (l3src.valid())
//	    {	
//		map <Level3Addr, Qualifier>::iterator mi = rep_l3src.find (l3src);
//		if (mi != rep_l3src.end()) {
//		    mi->second += Qualifier (packetlen);
//		} else {
//		    rep_l3src[l3src] = Qualifier (packetlen);
//		}
//		if (l3dst.valid())
//		{	
//		    map <Level3Addr, Qualifier>::iterator mi = rep_l3dst.find (l3dst);
//		    if (mi != rep_l3dst.end()) {
//			mi->second += Qualifier (packetlen);
//		    } else {
//			rep_l3dst[l3dst] = Qualifier (packetlen);
//		    }
//
//		    Level3AddrPair pair(l3src, l3dst);
//		    {	map <Level3AddrPair, Qualifier>::iterator mi = rep_l3pair.find(pair);
//			if (mi != rep_l3pair.end())
//			    mi->second += Qualifier (packetlen);
//			else
//			    rep_l3pair[pair] = Qualifier (packetlen);
//		    }
//		}
//	    }
	}
    }

    dump_desc_nb (rep_src_macaddr, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;
    dump_desc_len (rep_src_macaddr, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;

    dump_desc_nb (rep_dst_macaddr, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;
    dump_desc_len (rep_dst_macaddr, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;

    dump_desc_nb (rep_pair_macaddr, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;
    dump_desc_len (rep_pair_macaddr, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;

    dump_desc_nb (rep_ethertype, cout, Qualifier(nbpacket,totsize)); cout << endl;
    dump_desc_len (rep_ethertype, cout, Qualifier(nbpacket,totsize)); cout << endl;

    dump_desc_nb (rep_l3src, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;
    dump_desc_len (rep_l3src, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;

    dump_desc_nb (rep_l3dst, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;
    dump_desc_len (rep_l3dst, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;

    dump_desc_nb (rep_l3pair, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;
    dump_desc_len (rep_l3pair, cout, Qualifier(nbpacket,totsize), percent_ceil); cout << endl;

//    cout << "nb packet = " << nbpacket << endl;
//    cout << "nb src mac = " << rep_src_macaddr.size() << endl;
//    cout << "nb dst mac = " << rep_dst_macaddr.size() << endl;
//    cout << "nb pair mac = " << rep_pair_macaddr.size() << endl;
    cout << "tcpdump line read : " << lno << endl;
    return 0;
}




int main (void) {

    // // Level3Addr ipv6 (TETHER_IPV6, "2606:2800:1::5.53 > 2a00:1bb0::330:204:23ff:fecb:8768.3615:");
    // Level3Addr ipv6 (TETHER_IPV6, "2606:2800:1::5");
    // // Level3Addr ipv6 (TETHER_IPV6, "2a00:1bb0::330:204:23ff:fecb:8768.3615");
    // // Level3Addr ipv6 (TETHER_IPV6, "2001:db8:8714:3a90::12");
    // cout << " ipv6 = [" << ipv6 << "]" << endl;
    // return 0;

    return capstat (cin, cout);
}