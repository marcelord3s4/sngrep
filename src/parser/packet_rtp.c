/**************************************************************************
 **
 ** sngrep - SIP Messages flow viewer
 **
 ** Copyright (C) 2013-2019 Ivan Alonso (Kaian)
 ** Copyright (C) 2013-2019 Irontec SL. All rights reserved.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 ****************************************************************************/
/**
 * @file packet_rtp.c
 * @author Ivan Alonso [aka Kaian] <kaian@irontec.com>
 *
 * @brief Source of functions defined in packet_rtp.h
 *
 * @note RTP_VERSION and RTP_PAYLOAD_TYPE macros has been taken from wireshark
 *       source code: packet-rtp.c
 */

#include "config.h"
#include <glib.h>
#include "glib/glib-extra.h"
#include "storage/storage.h"
#include "parser/parser.h"
#include "parser/packet.h"
#include "packet_rtp.h"

// Version is the first 2 bits of the first octet
#define RTP_VERSION(octet) ((octet) >> 6)

// Payload type is the last 7 bits
#define RTP_PAYLOAD_TYPE(octet) (guint8)((octet) & 0x7F)

// Handled RTP versions
#define RTP_VERSION_RFC1889 2

/**
 * @brief Known RTP encodings
 * #encodings
 */
PacketRtpEncoding encodings[] = {
    { RTP_PT_PCMU,       "PCMU/8000",  "g711u", 8000 },
    { RTP_PT_GSM,        "GSM/8000",   "gsm",   8000 },
    { RTP_PT_G723,       "G723/8000",  "g723",  8000 },
    { RTP_PT_DVI4_8000,  "DVI4/8000",  "dvi",   8000 },
    { RTP_PT_DVI4_16000, "DVI4/16000", "dvi",   16000 },
    { RTP_PT_LPC,        "LPC/8000",   "lpc",   8000 },
    { RTP_PT_PCMA,       "PCMA/8000",  "g711a", 8000 },
    { RTP_PT_G722,       "G722/8000",  "g722",  8000 },
    { RTP_PT_L16_STEREO, "L16/44100",  "l16",   44100 },
    { RTP_PT_L16_MONO,   "L16/44100",  "l16",   44100 },
    { RTP_PT_QCELP,      "QCELP/8000", "qcelp", 8000 },
    { RTP_PT_CN,         "CN/8000",    "cn",    8000 },
    { RTP_PT_MPA,        "MPA/90000",  "mpa",   8000 },
    { RTP_PT_G728,       "G728/8000",  "g728",  8000 },
    { RTP_PT_DVI4_11025, "DVI4/11025", "dvi",   11025 },
    { RTP_PT_DVI4_22050, "DVI4/22050", "dvi",   22050 },
    { RTP_PT_G729,       "G729/8000",  "g729",  8000 },
    { RTP_PT_CELB,       "CelB/90000", "celb",  90000 },
    { RTP_PT_JPEG,       "JPEG/90000", "jpeg",  90000 },
    { RTP_PT_NV,         "nv/90000",   "nv",    90000 },
    { RTP_PT_H261,       "H261/90000", "h261",  90000 },
    { RTP_PT_MPV,        "MPV/90000",  "mpv",   90000 },
    { RTP_PT_MP2T,       "MP2T/90000", "mp2t",  90000 },
    { RTP_PT_H263,       "H263/90000", "h263",  90000 },
    { 0, NULL, NULL,                            0 },
};

PacketRtpData *
packet_rtp_data(const Packet *packet)
{
    g_return_val_if_fail(packet != NULL, NULL);

    // Get Packet rtp data
    PacketRtpData *rtp = g_ptr_array_index(packet->proto, PACKET_RTP);
    g_return_val_if_fail(rtp != NULL, NULL);

    return rtp;
}

PacketRtpEncoding *
packet_rtp_standard_codec(guint8 code)
{
    // Format from RTP codec id
    for (guint i = 0; encodings[i].format; i++) {
        if (encodings[i].id == code)
            return &encodings[i];
    }

    return NULL;
}

static GByteArray *
packet_rtp_parse(G_GNUC_UNUSED PacketParser *parser, Packet *packet, GByteArray *data)
{
    // Not enough data for a RTP packet
    if (data->len < sizeof(PacketRtpHdr))
        return data;

    PacketRtpHdr *hdr = (PacketRtpHdr *) data->data;
    // Validate RTP version field
    if (hdr->version != RTP_VERSION_RFC1889)
        return data;

    // Validate RTP payload type
    if (!(hdr->pt <= 64 || hdr->pt >= 96))
        return data;

    PacketRtpData *rtp = g_malloc0(sizeof(PacketRtpData));
    rtp->encoding = packet_rtp_standard_codec(hdr->pt);

    // Not standard payload_type, just set the id and let storage search in SDP rtpmap
    if (rtp->encoding == NULL) {
        rtp->encoding = g_malloc0(sizeof(PacketRtpEncoding));
        rtp->encoding->id = hdr->pt;
    }

    // Store RTP header information in host byte order
    rtp->seq = g_ntohs(hdr->seq);
    rtp->ts = g_ntohl(hdr->ts);
    rtp->ssrc = g_ntohl(hdr->ssrc);

    // Remove RTP headers from payload
    g_byte_array_remove_range(data, 0, 12);

    // Store RTP payload data
    rtp->payload = g_byte_array_ref(data);

    // Set packet RTP informaiton
    packet_add_type(packet, PACKET_RTP, rtp);

    // Add data to storage
    storage_add_packet(packet);

    return data;
}

static void
packet_rtp_free(G_GNUC_UNUSED PacketParser *parser, Packet *packet)
{
    g_return_if_fail(packet != NULL);
    PacketRtpData *rtp_data = g_ptr_array_index(packet->proto, PACKET_RTP);
    g_return_if_fail(rtp_data != NULL);

    if (packet_rtp_standard_codec(rtp_data->encoding->id) == NULL) {
        g_free(rtp_data->encoding);
    }

    g_byte_array_unref(rtp_data->payload);
    g_free(rtp_data);
}

PacketDissector *
packet_rtp_new()
{
    PacketDissector *proto = g_malloc0(sizeof(PacketDissector));
    proto->id = PACKET_RTP;
    proto->dissect = packet_rtp_parse;
    proto->free = packet_rtp_free;
    return proto;
}