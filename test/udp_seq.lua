package.path = package.path ..";?.lua;test/?.lua;app/?.lua;"
-- Lua uses '--' as comment to end of line read the
-- manual for more comment options.
require "Pktgen"
local seq_table = {			-- entries can be in any order
    ["eth_dst_addr"] = "00:16:31:f2:eb:8e",
    ["eth_src_addr"] = "00:16:31:f2:eb:8f",
    ["ip_dst_addr"] = "2.2.2.3",
    ["ip_src_addr"] = "2.2.2.2/24",	-- the 16 is the size of the mask value
    ["sport"] = 9898,			-- Standard port numbers
    ["dport"] = 19898,			-- Standard port numbers
    ["ethType"] = "ipv4",	-- ipv4|ipv6|vlan
    ["ipProto"] = "udp",	-- udp|tcp|icmp
    ["pktSize"] = 512,		-- 64 - 1518
    ["vlanid"] = 1,			-- 1 - 4095
    ["teid"] = 3,
    ["cos"] = 5,
    ["tos"] = 6
  };
-- seqTable( seq#, portlist, table );
pktgen.seqTable(0, "all", seq_table );
pktgen.set("all", "seq_cnt", 1);
