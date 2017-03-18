#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include "camkit.h"

#define WIDTH 640
#define HEIGHT 480
#define FRAMERATE 25

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("Usage: #simple_demo ip\n");
		return  -1;
	}

	struct cap_handle *caphandle = NULL;
	struct cvt_handle *cvthandle = NULL;
	struct enc_handle *enchandle = NULL;
	struct pac_handle *pachandle = NULL;
	struct net_handle *nethandle = NULL;
	struct cap_param capp;
	struct cvt_param cvtp;
	struct enc_param encp;
	struct pac_param pacp;
	struct net_param netp;

	// set paraments
	U32 vfmt = V4L2_PIX_FMT_YUYV;
	U32 ofmt = V4L2_PIX_FMT_YUV420;

	capp.dev_name = "/dev/video0";
	capp.width = WIDTH;
	capp.height = HEIGHT;
	capp.pixfmt = vfmt;
	capp.rate = FRAMERATE;

	cvtp.inwidth = WIDTH;
	cvtp.inheight = HEIGHT;
	cvtp.inpixfmt = vfmt;
	cvtp.outwidth = WIDTH;
	cvtp.outheight = HEIGHT;
	cvtp.outpixfmt = ofmt;

	encp.src_picwidth = WIDTH;
	encp.src_picheight = HEIGHT;
	encp.enc_picwidth = WIDTH;
	encp.enc_picheight = HEIGHT;
	encp.chroma_interleave = 0;
	encp.fps = FRAMERATE;
	encp.gop = 30;
	encp.bitrate = 800;

	pacp.max_pkt_len = 1400;
	pacp.ssrc = 10;

	netp.type = UDP;
	netp.serip = argv[1];
	//netp.serport = atoi(argv[2]);
	netp.serport = atoi("8888");

	caphandle = capture_open(capp);
	cvthandle = convert_open(cvtp);
	enchandle = encode_open(encp);
	pachandle = pack_open(pacp);
	nethandle = net_open(netp);
/////////////////////////////////////////////////////////////////////
	// start stream loop
	void *cap_buf, *cvt_buf, *hd_buf, *enc_buf, *pac_buf;
	int cap_len, cvt_len, hd_len, enc_len, pac_len;
	enum pic_t ptype;

	capture_start(caphandle);		// !!! need to start capture stream!

	while (1)
	{
		capture_get_data(caphandle, &cap_buf, &cap_len);		
		convert_do(cvthandle, cap_buf, cap_len, &cvt_buf, &cvt_len);	
		// fetch h264 headers first!
		while (encode_get_headers(enchandle, &hd_buf, &hd_len, &ptype) == 1)
		{	
			pack_put(pachandle, hd_buf, hd_len);
			while (pack_get(pachandle, &pac_buf, &pac_len) == 1)
			{
				net_send(nethandle, pac_buf, pac_len);
			}
		}

		encode_do(enchandle, cvt_buf, cvt_len, &enc_buf, &enc_len, &ptype);
		
		// RTP pack and send
		pack_put(pachandle, enc_buf, enc_len);
		while (pack_get(pachandle, &pac_buf, &pac_len) == 1)
		{
			net_send(nethandle, pac_buf, pac_len);
		}

	}
	capture_stop(caphandle);

	net_close(nethandle);
	pack_close(pachandle);
	encode_close(enchandle);
	convert_close(cvthandle);
	capture_close(caphandle);
	return 0;
}