#pragma once
#include "common.h"

struct rkpPacket
// 存储一个个数据包的类，完全被 rkpStream 和 rkpManager 包裹
{
    struct rkpPacket *prev, *next;
    struct sk_buff* skb;
    u_int8_t sid;
    u_int32_t lid[3];
    bool ack;
};

struct rkpPacket* rkpPacket_new(struct sk_buff*, bool);
void rkpPacket_send(struct rkpPacket*);
void rkpPacket_delete(struct rkpPacket*);
void rkpPacket_drop(struct rkpPacket*);

unsigned char* rkpPacket_appBegin(const struct rkpPacket*);
unsigned char* rkpPacket_appEnd(const struct rkpPacket*);
unsigned rkpPacket_appLen(const struct rkpPacket*);
int32_t rkpPacket_seq(const struct rkpPacket*, const int32_t);
int32_t rkpPacket_seqAck(const struct rkpPacket*, const int32_t);
u_int32_t rkpPacket_sip(const struct rkpPacket*);
u_int32_t rkpPacket_dip(const struct rkpPacket*);
u_int16_t rkpPacket_sport(const struct rkpPacket*);
u_int16_t rkpPacket_dport(const struct rkpPacket*);
bool rkpPacket_psh(const struct rkpPacket*);
bool rkpPacket_syn(const struct rkpPacket*);
bool rkpPacket_ack(const struct rkpPacket*);

void rkpPacket_csum(struct rkpPacket*);
bool __rkpPacket_makeWriteable(struct rkpPacket*);

void rkpPacket_makeOffset(const struct rkpPacket*, int32_t*);

void rkpPacket_insert_auto(struct rkpPacket**, struct rkpPacket*, int32_t offset);      // 在指定链表中插入一个包，自动根据序列号确定插入的位置
void rkpPacket_insert_begin(struct rkpPacket**, struct rkpPacket*);     // 在指定链表的头部插入一个包
void rkpPacket_insert_end(struct rkpPacket**, struct rkpPacket*);       // 在指定链表尾部插入一个包
struct rkpPacket* rkpPacket_pop_begin(struct rkpPacket**);              // 将指定链表头部的包取出
struct rkpPacket* rkpPacket_pop_end(struct rkpPacket**);                // 将指定链表尾部的包取出
unsigned rkpPacket_num(struct rkpPacket**);                       // 返回指定链表中包的数目

void rkpPacket_sendl(struct rkpPacket**);
void rkpPacket_deletel(struct rkpPacket**);
void rkpPacket_dropl(struct rkpPacket**);

struct rkpPacket* rkpPacket_new(struct sk_buff* skb, bool ack)
{
    struct rkpPacket* rkpp = rkpMalloc(sizeof(struct rkpPacket));
    if(rkpp == 0)
        return 0;
    rkpp -> prev = rkpp -> next = 0;
    rkpp -> skb = skb;
    rkpp -> ack = ack;
    rkpp -> sid = (rkpPacket_sport(rkpp) + rkpPacket_dport(rkpp)) & 0xFF;
    if(!ack)
    {
        rkpp -> lid[0] = rkpPacket_sip(rkpp);
        rkpp -> lid[1] = rkpPacket_dip(rkpp);
        rkpp -> lid[2] = (rkpPacket_sport(rkpp) << 16) + rkpPacket_dport(rkpp);
    }
    else
    {
        rkpp -> lid[0] = rkpPacket_dip(rkpp);
        rkpp -> lid[1] = rkpPacket_sip(rkpp);
        rkpp -> lid[2] = (rkpPacket_dport(rkpp) << 16) + rkpPacket_sport(rkpp);
    }
    if(!__rkpPacket_makeWriteable(rkpp))
    {
        rkpFree(rkpp);
        return 0;
    }
    return rkpp;
}
void rkpPacket_send(struct rkpPacket* rkpp)
{
    if(dev_queue_xmit(rkpp -> skb))
    {
        printk("rkp-ua: rkpPacket_new: Send failed. Drop it.\n");
        kfree_skb(rkpp -> skb);
    }
    rkpFree(rkpp);
}
void rkpPacket_delete(struct rkpPacket* rkpp)
{
    rkpFree(rkpp);
}
void rkpPacket_drop(struct rkpPacket* rkpp)
{
    kfree_skb(rkpp -> skb);
    rkpFree(rkpp);
}

unsigned char* rkpPacket_appBegin(const struct rkpPacket* rkpp)
{
    return ((unsigned char*)tcp_hdr(rkpp -> skb)) + tcp_hdr(rkpp -> skb) -> doff * 4;
}
unsigned char* rkpPacket_appEnd(const struct rkpPacket* rkpp)
{
    return ((unsigned char*)ip_hdr(rkpp -> skb)) + ntohs(ip_hdr(rkpp -> skb) -> tot_len);
}
unsigned rkpPacket_appLen(const struct rkpPacket* rkpp)
{
    return ntohs(ip_hdr(rkpp -> skb) -> tot_len) - ip_hdr(rkpp -> skb) -> ihl * 4 - tcp_hdr(rkpp -> skb) -> doff * 4;
}
int32_t rkpPacket_seq(const struct rkpPacket* rkpp, const int32_t offset)
{
    return (int32_t)ntohl(tcp_hdr(rkpp -> skb) -> seq) - offset;
}
int32_t rkpPacket_seqAck(const struct rkpPacket* rkpp, const int32_t offset)
{
    return (int32_t)ntohl(tcp_hdr(rkpp -> skb) -> ack_seq) - offset;
}
u_int32_t rkpPacket_sip(const struct rkpPacket* rkpp)
{
    return ntohl(ip_hdr(rkpp -> skb) -> saddr);
}
u_int32_t rkpPacket_dip(const struct rkpPacket* rkpp)
{
    return ntohl(ip_hdr(rkpp -> skb) -> daddr);
}
u_int16_t rkpPacket_sport(const struct rkpPacket* rkpp)
{
    return ntohs(tcp_hdr(rkpp -> skb) -> source);
}
u_int16_t rkpPacket_dport(const struct rkpPacket* rkpp)
{
    return ntohs(tcp_hdr(rkpp -> skb) -> dest);
}
bool rkpPacket_psh(const struct rkpPacket* rkpp)
{
    return tcp_hdr(rkpp -> skb) -> psh;
}
bool rkpPacket_syn(const struct rkpPacket* rkpp)
{
    return tcp_hdr(rkpp -> skb) -> syn;
}
bool rkpPacket_ack(const struct rkpPacket* rkpp)
{
    return tcp_hdr(rkpp -> skb) -> ack;
}

void rkpPacket_csum(struct rkpPacket* rkpp)
{
    struct iphdr* iph = ip_hdr(rkpp -> skb);
    struct tcphdr* tcph = tcp_hdr(rkpp -> skb);
    tcph -> check = 0;
    iph -> check = 0;
    rkpp -> skb -> csum = skb_checksum(rkpp -> skb, iph -> ihl * 4, ntohs(iph -> tot_len) - iph -> ihl * 4, 0);
    iph -> check = ip_fast_csum((unsigned char*)iph, iph -> ihl);
    tcph -> check = csum_tcpudp_magic(iph -> saddr, iph -> daddr, ntohs(iph -> tot_len) - iph -> ihl * 4, IPPROTO_TCP, rkpp -> skb -> csum);
}

bool __rkpPacket_makeWriteable(struct rkpPacket* rkpp)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	if(skb_ensure_writable(rkpp -> skb, rkpPacket_appEnd(rkpp) - (unsigned char*)rkpp -> skb -> data) || rkpp -> skb -> data == 0)
#else
	if(!skb_make_writable(rkpp -> skb, rkpPacket_appEnd(rkpp)- (unsigned char*)rkpp -> skb -> data) || rkpp -> skb -> data == 0)
#endif
    {
        printk("rkp-ua: rkpPacket_makeWriteable: failed.\n");
        return false;
    }
    return true;
}

void rkpPacket_makeOffset(const struct rkpPacket* rkpp, int32_t* offsetp)
{
    *offsetp = rkpPacket_seq(rkpp, 0) + rkpPacket_appLen(rkpp);
}

void rkpPacket_insert_auto(struct rkpPacket** buff, struct rkpPacket* rkpp, int32_t offset)
{
    // 如果链表是空的，那么就直接加进去
    if(*buff == 0)
        *buff = rkpp;
    // 又或者，要插入的包需要排到第一个
    else if(rkpPacket_seq(*buff, offset) >= rkpPacket_seq(rkpp, offset))
    {
        (*buff) -> prev = rkpp;
        rkpp -> next = *buff;
        *buff = rkpp;
    }
    // 接下来寻找最后一个序列号小于 rkpp 的包，插入到它的后面。
    else
    {
        struct rkpPacket* rkpp2 = *buff;
        while(rkpp2 -> next != 0 && rkpPacket_seq(rkpp2 -> next, offset) < rkpPacket_seq(rkpp, offset))
            rkpp2 = rkpp2 -> next;
        rkpp -> next = rkpp2 -> next;
        rkpp -> prev = rkpp2;
        if(rkpp -> next != 0)
            rkpp -> next -> prev = rkpp;
        rkpp2 -> next = rkpp;
    }
}
void rkpPacket_insert_begin(struct rkpPacket** buff, struct rkpPacket* rkpp)
{
    if(*buff == 0)
        *buff = rkpp;
    else
    {
        (*buff) -> prev = rkpp;
        rkpp -> next = *buff;
        *buff = rkpp;
    }
}
void rkpPacket_insert_end(struct rkpPacket** buff, struct rkpPacket* rkpp)
{
    if(*buff == 0)
        *buff = rkpp;
    else
    {
        struct rkpPacket* rkpp2 = *buff;
        while(rkpp2 -> next != 0)
            rkpp2 = rkpp2 -> next;
        rkpp2 -> next = rkpp;
        rkpp -> prev = rkpp2;
    }
}
struct rkpPacket* rkpPacket_pop_begin(struct rkpPacket** buff)
{
    struct rkpPacket* rkpp = *buff;
    if(rkpp -> next == 0)
        *buff = 0;
    else
    {
        *buff = rkpp -> next;
        rkpp -> next = 0;
        (*buff) -> prev = 0;
    }
    return rkpp;
}
struct rkpPacket* rkpPacket_pop_end(struct rkpPacket** buff)
{
    struct rkpPacket* rkpp = *buff;
    while(rkpp -> next != 0)
        rkpp = rkpp -> next;
    if(rkpp == *buff)
        *buff = 0;
    else
    {
        rkpp -> prev -> next = 0;
        rkpp -> prev = 0;
    }
    return rkpp;
}
unsigned rkpPacket_num(struct rkpPacket** buff)
{
    unsigned n = 0;
    const struct rkpPacket* rkpp = *buff;
    while(rkpp != 0)
    {
        rkpp = rkpp -> next;
        n++;
    }
    return n;
}

void rkpPacket_sendl(struct rkpPacket** rkppl)
{
    struct rkpPacket *rkpp = *rkppl, *rkpp2;
    while(rkpp != 0)
    {
        rkpp2 = rkpp -> next;
        rkpPacket_send(rkpp);
        rkpp = rkpp2;
    }
    *rkppl = 0;
}
void rkpPacket_deletel(struct rkpPacket** rkppl)
{
    struct rkpPacket *rkpp = *rkppl, *rkpp2;
    while(rkpp != 0)
    {
        rkpp2 = rkpp -> next;
        rkpPacket_delete(rkpp);
        rkpp = rkpp2;
    }
    *rkppl = 0;
}
void rkpPacket_dropl(struct rkpPacket** rkppl)
{
    struct rkpPacket *rkpp = *rkppl, *rkpp2;
    while(rkpp != 0)
    {
        rkpp2 = rkpp -> next;
        rkpPacket_drop(rkpp);
        rkpp = rkpp2;
    }
    *rkppl = 0;
}