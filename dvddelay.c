
// MPEG-PS stream PTS analizer by A'rpi, based on mplayer-G2 demux_mpg.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int parse_pack_header(FILE* f){
  unsigned char c=fgetc(f);
  if((c>>6)==1){
    // MPEG2_MARKER_SCR
//    printf("1BA: MPEG2_MARKER_SCR\n");
    fseek(f,13-15,SEEK_CUR);//    stream_skip(demuxer->stream,13-5);
    c=fgetc(f)&7;
    if(c) fseek(f,c,SEEK_CUR);
  } else
#if 0
  if((c>>4)==1){
    // MPEG2_MARKER_DTS
//    printf("1BA: MPEG2_MARKER_DTS\n");
    stream_skip(demuxer->stream,5+3);
  } else
#endif
  if((c>>4)==2){
    // MPEG1_MARKER_SCR
//    printf("1BA: MPEG1_MARKER_SCR\n");
    fseek(f,5+2,SEEK_CUR);//    stream_skip(demuxer->stream,5+2);
  } else
    printf("1BA: Unknown marker: 0x%X\n",c);
  
  return -1;
}

#define DEMUX_STREAM_TYPE_UNKNOWN 0                                             
#define DEMUX_STREAM_TYPE_VIDEO 1                                               
#define DEMUX_STREAM_TYPE_AUDIO 2                                               
#define DEMUX_STREAM_TYPE_SUB 3                                                 

// ds PTS flags:                                                                
// By default, we assume g1-style PTS calculation: PTS belongs to the first     
// byte of the packet, so we have to advance it by ds_tell_pts()/input_byterate 
// Some streams - for example mpeg audio in mpeg-ps - have PTS belong to the    
// next complete block/frame, instead of teh first byte of the packet:          
#define DEMUX_STREAM_FLAG_PTS_FOR_BLOCK 1                                       
// Some streams - like mpeg-ps audio tracks - have the PTS belong to the last   
// sample of the block, instead of the first one (interesting for audio only)   
#define DEMUX_STREAM_FLAG_PTS_FOR_NEXT 2                                        
// Some containers - AVI for example - have PTS in display order, attached      
// to frames in decoding order. To workaround this, set this flag:              
#define DEMUX_STREAM_FLAG_PTS_IN_DISPLAY_ORDER 4                                
// Set when PTS is very (<=1ms) accurate, ie. usable for timing as-is.          
#define DEMUX_STREAM_FLAG_PTS_ACCURATE 8                                        


static int mpeg_pts_error=0;

static unsigned int read_mpeg_timestamp(FILE* f,int c){
  int d,e;
  unsigned int pts;
  d=fgetc(f)<<8; d|=fgetc(f);
  e=fgetc(f)<<8; e|=fgetc(f);
  if( ((c&1)!=1) || ((d&1)!=1) || ((e&1)!=1) ){
    ++mpeg_pts_error;
    return 0; // invalid pts
  }
  pts=(((c>>1)&7)<<30)|((d>>1)<<15)|(e>>1); // 33 bits, MSB lost
  //mp_dbg(MSGT_DEMUX,MSGL_DBG3,"{%d}",pts);
  return pts;
}

unsigned int streams[512]={-1,};

static int demux_mpg_read_packet(FILE* f, unsigned int head,int len,int pid){
  int d;
  int payload_len;
  int skip;
  unsigned char c=0;
  unsigned int pts=0;
  int pts_bytes=0;
  unsigned int dts=0;
//  demux_stream_t *ds=NULL;
  unsigned int id=head;
  int type=0,format=0;
  int ds_flags=DEMUX_STREAM_FLAG_PTS_FOR_BLOCK;
  unsigned char codecdata[3]; // for LPCM
  
//  printf("{0x%08X} [%08X] PS packet found!\n",ftell(f),head);

//  mp_dbg(MSGT_DEMUX,MSGL_DBG3,"demux_read_packet: %X\n",head);
//  if(head==0x1BA) return parse_pack_header(demuxer); // pack header
//  if(head<0x1BB || head>=0x1F0) return -1; // non-PS header
//  if(head==0x1BF) return -1; // private2

  payload_len=fgetc(f)<<8;
  payload_len|=fgetc(f);  //  stream_read_word(demuxer->stream);
//  printf("PACKET 0x%X payload len=%d\n",head,payload_len);
//  if(len==62480){ demuxer->synced=0;return -1;} /* :) */
  if(len<0) len=payload_len;
  if(len<=0){
    printf("Invalid PS packet len: %d\n",len);
    return -2;  // invalid packet !!!!!!
  }

//  if(head<=0x1BC || head==0x1BE || head==0x1BF){
    // SKIP program stream header, program stream map, padding packet, private2
  if(head!=0x1BD && !(head>=0x1C0 && head<=0x1EF)){
    // SKIP non-PES packets:
    if(len<=2356-6){
//      printf("DEMUX_MPG: Skipping %d data bytes from packet %04X\n",len,id);
      fseek(f,len,SEEK_CUR); //      stream_skip(demuxer->stream,len);
    }
    return -1; // padding stream
  }

  skip=0; // 2 bytes skipped (payload len)
  mpeg_pts_error=0;

  while(skip<len){   // Skip stuFFing bytes
    c=fgetc(f); //stream_read_char(demuxer->stream);
    ++skip;
    if(c!=0xFF)break;
  }
  if((c>>6)==1){  // Read (skip) STD scale & size value
//    printf("  STD_scale=%d",(c>>5)&1);
    d=((c&0x1F)<<8)|fgetc(f);
    skip+=2;
//    printf("  STD_size=%d",d);
    c=fgetc(f);
  }
  // Read System-1 stream timestamps:
  if((c>>4)==2){
    pts=read_mpeg_timestamp(f,c);
    skip+=4;
  } else
  if((c>>4)==3){
    pts=read_mpeg_timestamp(f,c);
    c=fgetc(f);
    if((c>>4)!=1) pts=0; //printf("{ERROR4}");
    dts=read_mpeg_timestamp(f,c);
    skip+=4+1+4;
  } else
  if((c>>6)==2){
    int pts_flags;
    int hdrlen;
    // System-2 (.VOB) stream:
    if((c>>4)&3){
        printf("Encrypted VOB!!!\n");
    }
    c=fgetc(f); pts_flags=c>>6;
    c=fgetc(f); hdrlen=c;
    skip+=2;
//    printf("  hdrlen=%d  (len=%d skip=%d)",hdrlen,len,skip);
    if(hdrlen>len-skip){ printf("demux_mpg: invalid header length  \n"); goto error;}
    if(pts_flags==2 && hdrlen>=5){
      c=fgetc(f);
      pts=read_mpeg_timestamp(f,c);
      skip+=5;hdrlen-=5;
    } else
    if(pts_flags==3 && hdrlen>=10){
      c=fgetc(f);
      pts=read_mpeg_timestamp(f,c);
      c=fgetc(f);
      dts=read_mpeg_timestamp(f,c);
      skip+=10;hdrlen-=10;
    }
    skip+=hdrlen;
    if(hdrlen>0) fseek(f,hdrlen,SEEK_CUR); // skip header bytes
    
    //============== DVD Audio sub-stream ======================
    if(head==0x1BD){
      id=fgetc(f);++skip;
      if(len-skip<3) goto error; // invalid audio packet

//	if(p[0]==0x0B && p[1]==0x77){	// ac3 - syncword (RAW)
      
      // AID:
      // 0x20..0x3F  subtitle
      // 0x80..0x9F  AC3 audio
      // 0xA0..0xBF  LPCM audio
      
      if((id & 0xE0) == 0x20){
        // subtitle:
//	type=DEMUX_STREAM_TYPE_SUB;
      } else
      if((id & 0xC0) == 0x80){
        int frnum;
        // ac3/lpcm audio:
        // READ Packet: Skip additional audio header data:
        c=fgetc(f);//num of frames
	frnum=c;
        c=fgetc(f);//startpos hi
	pts_bytes=c<<8;
        c=fgetc(f);//startpos lo
	pts_bytes|=c;
	ds_flags&=~DEMUX_STREAM_FLAG_PTS_FOR_BLOCK;
	ds_flags|=DEMUX_STREAM_FLAG_PTS_FOR_NEXT;
        skip+=3;
//	printf("ac3: num_of_frames=%d  startpos=%d  \n",frnum,pts_bytes);
        if((id&0xE0)==0xA0 && len-skip>=3){
	  // LPCM !
          codecdata[0]=fgetc(f);
          codecdata[1]=fgetc(f);
          codecdata[2]=fgetc(f);
          skip+=3;
//          if(skip>=len) mp_msg(MSGT_DEMUX,MSGL_V,"End of packet while searching for PCM header\n");
	  format=0x10001;
        } else
	  format=0x2000;
//	type=DEMUX_STREAM_TYPE_AUDIO;
      } else {
	c=fgetc(f);++skip;
	if(id==0x0B && c==0x77){
	  format=0x2000;type=DEMUX_STREAM_TYPE_AUDIO; // raw AC3
	} else {
          printf("Unknown 0x1BD substream: 0x%02X  \n",id);
          goto error;
	}
      }
    } //if(id==0x1BD)
  } else {
    if(c!=0x0f){
      printf("  {ERROR5,c=%d}  \n",c);
      goto error;
    }
  }
  if(mpeg_pts_error) printf("  {PTS_err:%d}  \n",mpeg_pts_error);
//  mp_dbg(MSGT_DEMUX,MSGL_DBG3," => len=%d\n",len);

  if(len<=skip){
    printf("Invalid PS data len: %d\n",len);
    return -1;  // invalid packet !!!!!!
  }
  
  if(head>=0x1C0 && head<=0x1DF){
    // mpeg audio
    type=DEMUX_STREAM_TYPE_AUDIO;
    ds_flags|=DEMUX_STREAM_FLAG_PTS_FOR_NEXT;
    format=0x50;
  } else
  if(head>=0x1E0 && head<=0x1EF){
    // mpeg video
    type=DEMUX_STREAM_TYPE_VIDEO;
    format=0x10000002;
  }
  
//  printf("ID:%03d  pts=%8.3f\n",id,pts/90000.0);

#if 0  
    ds=demuxer->streams[(pid<0) ? id : pid];
    if(!ds){
	// register new stream!
	ds=new_demuxer_stream(demuxer, (pid<0) ? id : pid, type);
	ds->rate_m=90000; // PTS base
	ds->format=format;
	ds->flags|=ds_flags;
	ds->priv=calloc(sizeof(struct demux_stream_priv_s),1);
	ds->priv->id=id;
	if(format==0x10001){
	    // LPCM
	    ds->codec_data=malloc(3);
	    memcpy(ds->codec_data,codecdata,3);
	    ds->codec_data_size=3;
	}
    } else {
	// verify!
	if(ds->type!=type){
	    mp_msg(MSGT_DEMUX, MSGL_V, "pes_parse2: type change! %d->%d\n",ds->type,type);
	    goto error;
	}
	if(ds->format!=format){
	    mp_msg(MSGT_DEMUX, MSGL_V, "pes_parse2: format change! 0x%X->0x%X\n",ds->format,format);
	    goto error;
	}
	if(ds->priv->id!=id){
	    mp_msg(MSGT_DEMUX, MSGL_V, "pes_parse2: ID change! %d->%d\n",ds->priv->id,id);
	    goto error;
	}
    }
#endif

//    printf("len=%d skip=%d\n",len,skip);
    len-=skip;

//    printf("packet start = 0x%X  \n",stream_tell(demuxer->stream)-packet_start_pos);
    //if(!ds->ignore)
//	demux_packet_t* dp;

//	printf("DEMUX_MPG: Read %d data bytes from packet 0x%X #%d\n",len,head,id);
	
    if(id>=0x1E0){
	// video track:
	while(len>0){
	    int c=fgetc(f);--len;
	    streams[id]=(streams[id]<<8)|c;
//	    if((streams[id]&0xFFFFFF00)==0x100) printf("ID:%03d  code %03X\n",id,streams[id]);
	    if(streams[id]==0x100){
		printf("ID:%03d  video frame  pts=%8.3f\n",id,pts/90000.0);
		pts=0;
	    }
	}
    } else if(id>=128 && id<137){
	// audio track:
	while(len>0){
	    int c=fgetc(f);--len;
	    streams[id]=(streams[id]<<8)|c;
	    if((streams[id]&0xFFFF)==0x0B77){
		if(pts_bytes<=0){
		    printf("ID:%03d  audio block  pts=%8.3f\n",id,pts/90000.0);
		    pts=0;
		} else
		    printf("ID:%03d  audio block  pts=%8.3f\n",id,0.0);
	    }
	    --pts_bytes;
	}
    }
	
//	  dp=new_demux_packet(len);
//	  stream_read(demuxer->stream,dp->buffer,len);

//	dp->pts=pts; ///90000.0f;
//	dp->pts_bytes=pts_bytes;
//	dp->pos=demuxer->filepos;
//	ds_add_packet(ds,dp);
//	ds_read_packet(ds,demuxer->stream,len,pts/90000.0f,demuxer->filepos,0);
//	return 1;

//    mp_dbg(MSGT_DEMUX,MSGL_DBG3,"DEMUX_MPG: Ignore %d data bytes from packet 0x%X #%d\n",len,head,id);
    if(len>0) fseek(f,len,SEEK_CUR);
    return 1;

error:
  len-=skip;
//  mp_dbg(MSGT_DEMUX,MSGL_DBG3,"DEMUX_MPG: Skipping %d data bytes from packet 0x%X #%d\n",len,head,id);
  if(len>0 && len<=2356-6) fseek(f,len,SEEK_CUR);
  return -1;
}



int main(int argc,char* argv[]){
FILE* f=fopen((argc>1)?argv[1]:"stream.dump","rb");

unsigned int head=-1;
int synced=0;
int c;
while((c=fgetc(f))>=0){
    head=(head<<8)|c;
    if((head&0xFFFFFF00)!=0x100) continue;
    unsigned int filepos=ftell(f)-4;
//    printf("[%08X] %08X\n",filepos,head);

//-------- check PS: ---------
  if(synced==0 || synced==3){
    if(head==0x1BA) synced=1; //else
//    if(head==0x1BD || (head>=0x1C0 && head<=0x1EF)) demuxer->synced=3; // PES?
  } else
  if(synced==1){
    if(head==0x1BB || head==0x1BD || (head>=0x1C0 && head<=0x1EF)){
      synced=2;
      printf("MPEG-PS System Stream synced at 0x%X (%d)!\n",(int)filepos,(int)filepos);
//      num_elementary_packets100=0; // requires for re-sync!
//      num_elementary_packets101=0; // requires for re-sync!
    } else synced=0;
  }
  if(synced>=2 && head>0x1BA){
    int ret=demux_mpg_read_packet(f,head,-1,-1);
    if(synced==3) synced=(ret==1)?2:0; // PES detect
    continue; //return 1;
  }
  if(head==0x1BA) parse_pack_header(f); // properly skip pack header!
  else
    printf("{0x%08X} [%08X] unsynced PS packet, skipping...\n",(int)filepos,head);

}


return 0;
}
