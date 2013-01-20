#include "sglist.hh"
#include "debug.hh"

void
sglist::dump() {
    debug(fmt("nsgs=%d, max=%d") % _nsgs % _max_sgs);
    for (auto i = _nodes.begin(); i != _nodes.end(); i++) {
        debug(fmt("\t paddr=%x, len=%d") % i->_paddr, i->_len);
    }
}

bool
sglist::add(u64 paddr, u32 len) {
    if (_nsgs == max_sgs)
        return false;

    sg_node n(paddr, len);
    _nodes.push_back(n);
    _nsgs++;

    return true;
}