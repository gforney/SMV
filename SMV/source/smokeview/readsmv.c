// $Date$ 
// $Revision$
// $Author$

// svn revision character string
char readsmv_revision[]="$Revision$";

#include "options.h"
#ifdef pp_GLEW
#include "glew.h"
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef pp_OSX
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <pthread.h>

#include "string_util.h"
#include "smv_endian.h"
#include "update.h"
#include "smokeviewvars.h"
#include "IOvolsmoke.h"

#define BREAK \
      if((stream==stream1&&stream2==NULL)||stream==stream2)break;\
      stream=stream2;\
      continue

#define COLOR_INVISIBLE -2

#define BLOCK_OUTLINE 2

#define DEVICE_DEVICE 0
#define DEVICE_HEAT 2     
#define DEVICE_SPRK 3
#define DEVICE_SMOKE 4

propdata *get_prop_id(char *prop_id);
void init_evac_prop(void);
void init_prop(propdata *propi, int nsmokeview_ids, char *label);
void getsliceparams2(void);

/* ------------------ init_default_prop ------------------------ */

void init_default_prop(void){
/*
PROP
 Human_props
  4
 human_fixed
 human_altered_with_data
 ellipsoid
 disk
  1
 D=0.2
 */
  propdata *propi;
  char proplabel[255];
  int lenbuf;
  int ntextures_local;
  int nsmokeview_ids;
  char *smokeview_id;
  char buffer[255];
  int i;
  
  propi = propinfo + npropinfo;

  strcpy(proplabel,"Human_props(default)");           // from input

  nsmokeview_ids=4;                                   // from input

  init_prop(propi,nsmokeview_ids,proplabel);
  for(i=0;i<nsmokeview_ids;i++){
    if(i==0)strcpy(buffer,"human_fixed");             // from input
    if(i==1)strcpy(buffer,"human_altered_with_data"); // from input
    if(i==2)strcpy(buffer,"ellipsoid");               // from input
    if(i==3)strcpy(buffer,"disk");                    // from input
    lenbuf=strlen(buffer);
    NewMemory((void **)&smokeview_id,lenbuf+1);
    strcpy(smokeview_id,buffer);
    propi->smokeview_ids[i]=smokeview_id;
    propi->smv_objects[i]=get_SVOBJECT_type(propi->smokeview_ids[i],missing_device);
  }
  propi->smv_object=propi->smv_objects[0];
  propi->smokeview_id=propi->smokeview_ids[0];

  propi->nvars_indep=1;                               // from input
  propi->vars_indep=NULL;
  propi->svals=NULL;
  propi->texturefiles=NULL;
  ntextures_local=0;
  if(propi->nvars_indep>0){
    NewMemory((void **)&propi->vars_indep,propi->nvars_indep*sizeof(char *));
    NewMemory((void **)&propi->svals,propi->nvars_indep*sizeof(char *));
    NewMemory((void **)&propi->fvals,propi->nvars_indep*sizeof(float));
    NewMemory((void **)&propi->vars_indep_index,propi->nvars_indep*sizeof(int));
    NewMemory((void **)&propi->texturefiles,propi->nvars_indep*sizeof(char *));

    for(i=0;i<propi->nvars_indep;i++){
      char *equal;

      propi->svals[i]=NULL;
      propi->vars_indep[i]=NULL;
      propi->fvals[i]=0.0;
      if(i==0)strcpy(buffer,"D=0.2");                 // from input
      equal=strchr(buffer,'=');
      if(equal!=NULL){
        char *buf1, *buf2, *keyword, *val;
        int lenkey, lenval;
        char *texturefile;

        buf1=buffer;
        buf2=equal+1;
        *equal=0;

        trim(buf1);
        keyword=trim_front(buf1);
        lenkey=strlen(keyword);

        trim(buf2);
        val=trim_front(buf2);
        lenval=strlen(val);

        if(lenkey==0||lenval==0)continue;

        if(val[0]=='"'){
          val[0]=' ';
          if(val[lenval-1]=='"')val[lenval-1]=' ';
          trim(val);
          val=trim_front(val);
          NewMemory((void **)&propi->svals[i],lenval+1);
          strcpy(propi->svals[i],val);
          texturefile=strstr(val,"t%");
          if(texturefile!=NULL){
            texturefile+=2;
            texturefile=trim_front(texturefile);
            propi->texturefiles[ntextures_local]=propi->svals[i];
            strcpy(propi->svals[i],texturefile);

            ntextures_local++;
          }
        }

        NewMemory((void **)&propi->vars_indep[i],lenkey+1);
        strcpy(propi->vars_indep[i],keyword);
        sscanf(val,"%f",propi->fvals+i);
      }
    }
    get_indep_var_indices(propi->smv_object,propi->vars_indep,propi->nvars_indep,propi->vars_indep_index);
    get_evac_indices(propi->smv_object,propi->fvars_evac_index,&propi->nvars_evac);
  }
  propi->ntextures=ntextures_local;
}

/* ------------------ update_inilist ------------------------ */

void update_inilist(void){
  FILE *filein_inilist=NULL;
  char ini_listfile[1024];
  char buffer[255],buffer2[255];

  strcpy(ini_listfile,caseinifilename);
  strcat(ini_listfile,"list");
  filein_inilist=fopen(ini_listfile,"r");
  if(filein_inilist!=NULL){

    while(!feof(filein_inilist)){
      CheckMemory;
      if(fgets(buffer,255,filein_inilist)==NULL)break;
      if(match(buffer,"INIFILE")==1){
        if(fgets(buffer2,255,filein_inilist)==NULL)break;
        cleanbuffer(buffer,buffer2);
        insert_inifile(buffer);
        continue;
        }
    }
    fclose(filein_inilist);
  }
}

/* ------------------ propi ------------------------ */

void init_prop(propdata *propi, int nsmokeview_ids, char *label){
  int nlabel;

  nlabel = strlen(label);
  if(nlabel==0){
    NewMemory((void **)&propi->label,5);
    strcpy(propi->label,"null");
  }
  else{
    NewMemory((void **)&propi->label,nlabel+1);
    strcpy(propi->label,label);
  }
  
  NewMemory((void **)&propi->smokeview_ids,nsmokeview_ids*sizeof(char *));
  NewMemory((void **)&propi->smv_objects,nsmokeview_ids*sizeof(sv_object *));

  propi->nsmokeview_ids=nsmokeview_ids;
  propi->blockvis=1;
  propi->inblockage=0;
  propi->ntextures=0;
  propi->nvars_dep=0;
  propi->nvars_evac=0;
  propi->nvars_indep=0;
  propi->vars_indep=NULL;
  propi->svals=NULL;
  propi->texturefiles=NULL;
  propi->rotate_axis=NULL;
  propi->rotate_angle=0.0;
}

/* ------------------ readsmv_dynamic ------------------------ */

void readsmv_dynamic(char *file){
  char buffer[255],buffer2[255];
  FILE *stream;
  int ioffset;
  float time_local;
  int nn;
  blockagedata *bc;
  int i,j;
  int nn_plot3d=0,iplot3d=0;
  int do_pass2=0, do_pass3=0, minmaxpl3d=0;
  int nplot3dinfo_old;

  nplot3dinfo_old=nplot3dinfo;

  updatefacelists=1;
  updatemenu=1;
  if(nplot3dinfo>0){
    int n;

    for(i=0;i<nplot3dinfo;i++){
      plot3ddata *plot3di;

      plot3di = plot3dinfo + i;
      for(n=0;n<6;n++){
        freelabels(&plot3di->label[n]);
      }
      FREEMEMORY(plot3di->reg_file);
      FREEMEMORY(plot3di->comp_file);
    }
//    FREEMEMORY(plot3dinfo);
  }
  nplot3dinfo=0;

  stream=fopen(file,"r");
  if(stream==NULL)return;
  for(i=0;i<nmeshes;i++){
    mesh *meshi;

    meshi=meshinfo+i;
    meshi->nsmoothblockages_list=1;
    FREEMEMORY(meshi->smoothblockages_list);
    for(j=0;j<meshi->nbptrs;j++){
      blockagedata *bc_local;

      bc_local=meshi->blockageinfoptrs[j];
      bc_local->nshowtime=0;
      FREEMEMORY(bc_local->showtime);
      FREEMEMORY(bc_local->showhide);
    }
    for(j=0;j<meshi->nvents;j++){
      ventdata *vi;

      vi = meshi->ventinfo + j;
      vi->nshowtime=0;
      FREEMEMORY(vi->showhide);
      FREEMEMORY(vi->showtime);
    }
  }
  for(i=ndeviceinfo_exp;i<ndeviceinfo;i++){
    devicedata *devicei;

    devicei = deviceinfo + i;
    devicei->nstate_changes=0;
    FREEMEMORY(devicei->act_times);
    FREEMEMORY(devicei->state_values);
  }

  ioffset=0;

  // ------------------------------- pass 1 dynamic - start ------------------------------------

  for(;;){
    if(fgets(buffer,255,stream)==NULL)break;
    if(strncmp(buffer," ",1)==0||buffer[0]==0)continue;
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ OFFSET ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"OFFSET") == 1){
      ioffset++;
      continue;
    }
/*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ PL3D ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

    if(match(buffer,"PL3D") == 1){
      do_pass2=1;
      nplot3dinfo++;
      continue;
   
    }
/*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ OPEN_VENT ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"OPEN_VENT") == 1||match(buffer,"CLOSE_VENT")==1){
      mesh *meshi;
      int len;
      ventdata *vi;
      int showvent, blocknumber, tempval;
  
      do_pass2=1;
      showvent=1;
      if(match(buffer,"CLOSE_VENT") == 1)showvent=0;
      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      trim(buffer);
      len=strlen(buffer);
      if(showvent==1){
        if(len>10){
          sscanf(buffer+10,"%i",&blocknumber);
          blocknumber--;
          if(blocknumber<0)blocknumber=0;
          if(blocknumber>nmeshes-1)blocknumber=nmeshes-1;
        }
      }
      else{
        if(len>11){
          sscanf(buffer+11,"%i",&blocknumber);
          blocknumber--;
          if(blocknumber<0)blocknumber=0;
          if(blocknumber>nmeshes-1)blocknumber=nmeshes-1;
        }
      }
      meshi=meshinfo + blocknumber;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f",&tempval,&time_local);
      tempval--;
      if(meshi->ventinfo==NULL||tempval<0||tempval>=meshi->nvents)continue;
      nn=tempval;
      vi=meshi->ventinfo+nn;
      vi->nshowtime++;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SHOW_OBST ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SHOW_OBST") == 1||match(buffer,"HIDE_OBST")==1){
      mesh *meshi;
      int blocknumber,blocktemp,showobst,tempval;

      do_pass2=1;
      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(strlen(buffer)>10){
        sscanf(buffer,"%s %i",buffer2,&blocktemp);
        if(blocktemp>0&&blocktemp<=nmeshes)blocknumber = blocktemp-1;
      }
      showobst=0;
      if(match(buffer,"SHOW_OBST") == 1)showobst=1;
      meshi=meshinfo + blocknumber;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f",&tempval,&time_local);
      tempval--;
      if(tempval<0||tempval>=meshi->nbptrs)continue;
      nn=tempval;
      bc=meshi->blockageinfoptrs[nn];
      bc->nshowtime++;
      meshi->nsmoothblockages_list++;
      if(meshi->nsmoothblockages_list>0)use_menusmooth=1;
      continue;
    }

  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ DEVICE_ACT +++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"DEVICE_ACT") == 1){
      devicedata *devicei;
      int idevice;
      float act_time;
      int act_state;

      do_pass2=1;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i",&idevice,&act_time,&act_state);
      idevice--;
      if(idevice>=0&&idevice<ndeviceinfo){
        devicei = deviceinfo + idevice;
        devicei->act_time=act_time;
        devicei->nstate_changes++;
      }

      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ HEAT_ACT +++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"HEAT_ACT") == 1){
      mesh *meshi;
      int blocknumber,blocktemp;

      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(strlen(buffer)>9){
        sscanf(buffer,"%s %i",buffer2,&blocktemp);
        if(blocktemp>0&&blocktemp<=nmeshes)blocknumber = blocktemp-1;
      }
      meshi=meshinfo + blocknumber;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f",&nn,&time_local);
      if(meshi->theat!=NULL && nn>=1 && nn <= meshi->nheat){
        int idev;
        int count=0;

        meshi->theat[nn-1]=time_local;
        for(idev=0;idev<ndeviceinfo;idev++){
          devicedata *devicei;

          devicei = deviceinfo + idev;
          if(devicei->type==DEVICE_HEAT){
            count++;
            if(nn==count){
              devicei->act_time=time_local;
              break;
            }
          }
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SPRK_ACT ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SPRK_ACT") == 1){
      mesh *meshi;
      int blocknumber,blocktemp;

      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(strlen(buffer)>9){
        sscanf(buffer,"%s %i",buffer2,&blocktemp);
        if(blocktemp>0&&blocktemp<=nmeshes)blocknumber = blocktemp-1;
      }
      meshi=meshinfo + blocknumber;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f",&nn,&time_local);
      if(meshi->tspr!=NULL && nn <= meshi->nspr && nn > 0){
        int idev;
        int count=0;

        meshi->tspr[nn-1]=time_local;

        for(idev=0;idev<ndeviceinfo;idev++){
          devicedata *devicei;

          devicei = deviceinfo + idev;
          if(devicei->type==DEVICE_SPRK){
            count++;
            if(nn==count){
              devicei->act_time=time_local;
              break;
            }
          }
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SMOD_ACT ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SMOD_ACT") == 1){
      int idev;
      int count=0;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f",&nn,&time_local);
      for(idev=0;idev<ndeviceinfo;idev++){
        devicedata *devicei;

        devicei = deviceinfo + idev;
        if(devicei->type==DEVICE_SMOKE){
          count++;
          if(nn==count){
            devicei->act_time=time_local;
            break;
          }
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ MINMAXPL3D +++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"MINMAXPL3D") == 1){
      minmaxpl3d=1;
      do_pass2=1;
      continue;
    }
  }

  // ------------------------------- pass 1 dynamic - end ------------------------------------

  for(i=0;i<nmeshes;i++){
    mesh *meshi;
    int nlist;

    meshi=meshinfo+i;

    nlist=meshi->nsmoothblockages_list+2; // add an entry for t=0.0
    FREEMEMORY(meshi->smoothblockages_list);
    NewMemory((void **)&meshi->smoothblockages_list,nlist*sizeof(smoothblockage));

    meshi->nsmoothblockages_list++;

    for(j=0;j<nlist;j++){
      smoothblockage *sb;

      sb=meshi->smoothblockages_list+j;
      sb->smoothblockagecolors=NULL;
      sb->smoothblockagesurfaces=NULL;
      sb->nsmoothblockagecolors=0;
    }
    meshi->nsmoothblockages_list=1;
    meshi->smoothblockages_list[0].time=-1.0;
    meshi->nsmoothblockages_list++;
  }
  if(nplot3dinfo>0){
    if(plot3dinfo==NULL){
      NewMemory((void **)&plot3dinfo,nplot3dinfo*sizeof(plot3ddata));
    }
    else{
      ResizeMemory((void **)&plot3dinfo,nplot3dinfo*sizeof(plot3ddata));
    }
  }
  for(i=0;i<ndeviceinfo;i++){
    devicedata *devicei;

    devicei = deviceinfo + i;
    devicei->istate_changes=0;
  }

  ioffset=0;
  rewind(stream);

  // ------------------------------- pass 2 dynamic - start ------------------------------------

  while(do_pass2==1){
    if(fgets(buffer,255,stream)==NULL)break;
    if(strncmp(buffer," ",1)==0||buffer[0]==0)continue;
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ OFFSET ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"OFFSET") == 1){
      ioffset++;
      continue;
    }
    /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ PL3D ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"PL3D") == 1){
      plot3ddata *plot3di;
      int len,blocknumber,blocktemp;
      char *bufferptr;

      if(minmaxpl3d==1)do_pass3=1;
      nn_plot3d++;
      trim(buffer);
      len=strlen(buffer);
      blocknumber = 0;
      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(strlen(buffer)>5){
        sscanf(buffer,"%s %f %i",buffer2,&time_local,&blocktemp);
        if(blocktemp>0&&blocktemp<=nmeshes)blocknumber = blocktemp-1;
      }
      else{
        time_local=-1.0;
      }
      if(fgets(buffer,255,stream)==NULL){
        nplot3dinfo--;
        break;
      }
      bufferptr=trim_string(buffer);
      len=strlen(bufferptr);

      plot3di=plot3dinfo+iplot3d;
      plot3di->blocknumber=blocknumber;
      plot3di->seq_id=nn_plot3d;
      plot3di->autoload=0;
      plot3di->time=time_local;
      if(plot3di>plot3dinfo+nplot3dinfo_old-1){
        plot3di->loaded=0;
        plot3di->display=0;
      }

      NewMemory((void **)&plot3di->reg_file,(unsigned int)(len+1));
      STRCPY(plot3di->reg_file,bufferptr);

      NewMemory((void **)&plot3di->comp_file,(unsigned int)(len+4+1));
      STRCPY(plot3di->comp_file,bufferptr);
      STRCAT(plot3di->comp_file,".svz");

      if(file_exists(plot3di->comp_file)==1){
        plot3di->compression_type=1;
        plot3di->file=plot3di->comp_file;
      }
      else{
        plot3di->compression_type=0;
        plot3di->file=plot3di->reg_file;
      }
      //disable compression for now
      plot3di->compression_type=0;
      plot3di->file=plot3di->reg_file;

      if(file_exists(plot3di->file)==0){
        int n;

        for(n=0;n<5;n++){
          if(readlabels(&plot3di->label[n],stream)==2)return;
        }
        nplot3dinfo--;
      }
      else{
        int n;

        plot3di->u = -1;
        plot3di->v = -1;
        plot3di->w = -1;
        for(n=0;n<5;n++){
          char *UVEL="U-VEL", *VVEL="V-VEL", *WVEL="W-VEL";

          if(readlabels(&plot3di->label[n],stream)==2)return;
          if(match(plot3di->label[n].shortlabel,UVEL) == 1){
            plot3di->u = n;
          }
          if(match(plot3di->label[n].shortlabel,VVEL) == 1){
            plot3di->v = n;
          }
          if(match(plot3di->label[n].shortlabel,WVEL) == 1){
            plot3di->w = n;
          }
        }
        if(plot3di->u>-1||plot3di->v>-1||plot3di->w>-1){
          plot3di->nvars=mxplot3dvars;
        }
        else{
          plot3di->nvars=5;
        }
        if(NewMemory((void **)&plot3di->label[5].longlabel,6)==0)return;
        if(NewMemory((void **)&plot3di->label[5].shortlabel,6)==0)return;
        if(NewMemory((void **)&plot3di->label[5].unit,4)==0)return;

        STRCPY(plot3di->label[5].longlabel,"Speed");
        STRCPY(plot3di->label[5].shortlabel,"Speed");
        STRCPY(plot3di->label[5].unit,"m/s");

        STRCPY(plot3di->longlabel,"");
        for(n=0;n<5;n++){
          STRCAT(plot3di->longlabel,plot3di->label[n].shortlabel);
          if(n!=4)STRCAT(plot3di->longlabel,", ");
        }

        iplot3d++;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ OPEN_VENT ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"OPEN_VENT") == 1||match(buffer,"CLOSE_VENT")==1){
      mesh *meshi;
      int len,showvent,blocknumber,tempval;
      ventdata *vi;

      showvent=1;
      if(match(buffer,"CLOSE_VENT") == 1)showvent=0;
      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      trim(buffer);
      len=strlen(buffer);
      if(showvent==1){
        if(len>10){
          sscanf(buffer+10,"%i",&blocknumber);
          blocknumber--;
          if(blocknumber<0)blocknumber=0;
          if(blocknumber>nmeshes-1)blocknumber=nmeshes-1;
        }
      }
      else{
        if(len>11){
          sscanf(buffer+11,"%i",&blocknumber);
          blocknumber--;
          if(blocknumber<0)blocknumber=0;
          if(blocknumber>nmeshes-1)blocknumber=nmeshes-1;
        }
      }
      meshi=meshinfo + blocknumber;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f",&tempval,&time_local);
      tempval--;
      if(meshi->ventinfo==NULL)continue;
      if(tempval<0||tempval>=meshi->nvents)continue;
      nn=tempval;
      vi=meshi->ventinfo+nn;
      if(vi->showtime==NULL){
        NewMemory((void **)&vi->showtime,(vi->nshowtime+1)*sizeof(float));
        NewMemory((void **)&vi->showhide,(vi->nshowtime+1)*sizeof(unsigned char));
        vi->nshowtime=1;
        vi->showtime[0]=0.0;
        vi->showhide[0]=1;
      }
      if(showvent==1){
        vi->showhide[vi->nshowtime]=1;
      }
      else{
        vi->showhide[vi->nshowtime]=0;
      }
      vi->showtime[vi->nshowtime++]=time_local;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SHOW_OBST ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SHOW_OBST") == 1||match(buffer,"HIDE_OBST")==1){
      mesh *meshi;
      int nlist,blocknumber,tempval,showobst,blocktemp;

      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(strlen(buffer)>10){
        sscanf(buffer,"%s %i",buffer2,&blocktemp);
        if(blocktemp>0&&blocktemp<=nmeshes)blocknumber = blocktemp-1;
      }
      showobst=0;
      if(match(buffer,"SHOW_OBST") == 1)showobst=1;
      meshi=meshinfo + blocknumber;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f",&tempval,&time_local);
      tempval--;
      if(tempval<0||tempval>=meshi->nbptrs)continue;
      nn=tempval;
      bc=meshi->blockageinfoptrs[nn];

      meshi->nsmoothblockages_list++;
      nlist=meshi->nsmoothblockages_list;
      if(nlist==2&&time_local!=0.0){          //  insert time=0.0 into list if not there
        meshi->smoothblockages_list[1].time=0.0;
        meshi->nsmoothblockages_list++;
        nlist++;
      }
      meshi->smoothblockages_list[nlist-1].time=time_local;
      if(time_local==meshi->smoothblockages_list[nlist-2].time){
        nlist--;
        meshi->nsmoothblockages_list=nlist;
      }

      if(bc->showtime==NULL){
        if(time_local!=0.0)bc->nshowtime++;
        NewMemory((void **)&bc->showtime,bc->nshowtime*sizeof(float));
        NewMemory((void **)&bc->showhide,bc->nshowtime*sizeof(unsigned char));
        bc->nshowtime=0;
        if(time_local!=0.0){
          bc->nshowtime=1;
          bc->showtime[0]=0.0;
          if(showobst==1){
            bc->showhide[0]=0;
          }
          else{
            bc->showhide[0]=1;
          }
        }
      }
      bc->nshowtime++;
      if(showobst==1){
        bc->showhide[bc->nshowtime-1]=1;
      }
      else{
        bc->showhide[bc->nshowtime-1]=0;
      }
      bc->showtime[bc->nshowtime-1]=time_local;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ DEVICE_ACT ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"DEVICE_ACT") == 1){
      devicedata *devicei;
      int idevice;
      float act_time;
      int act_state=1;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i",&idevice,&act_time,&act_state);
      idevice--;
      if(idevice>=0&&idevice<ndeviceinfo){
        int istate;

        devicei = deviceinfo + idevice;
        devicei->act_time=act_time;
        if(devicei->act_times==NULL){
          devicei->nstate_changes++;
          NewMemory((void **)&devicei->act_times,devicei->nstate_changes*sizeof(int));
          NewMemory((void **)&devicei->state_values,devicei->nstate_changes*sizeof(int));
          devicei->act_times[0]=0.0;
          devicei->state_values[0]=devicei->state0;
          devicei->istate_changes++;
        }
        istate = devicei->istate_changes++;
        devicei->act_times[istate]=act_time;
        devicei->state_values[istate]=act_state;
      }
      continue;
    }

  }

  // ------------------------------- pass 2 dynamic - end ------------------------------------

  rewind(stream);

  // ------------------------------- pass 3 dynamic - start ------------------------------------

  while(do_pass3==1){
    if(fgets(buffer,255,stream)==NULL)break;
    if(strncmp(buffer," ",1)==0||buffer[0]==0)continue;
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ MINMAXPL3D +++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"MINMAXPL3D") == 1){
      char *file_ptr, file2[1024];
      float valmin[5], valmax[5];
      float percentile_min[5], percentile_max[5];

      fgets(buffer,255,stream);
      strcpy(file2,buffer);
      file_ptr = file2;
      trim(file2);
      file_ptr = trim_front(file2);

      for(i=0;i<5;i++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f %f",valmin +i,valmax+i, percentile_min+i,percentile_max+i);
      }

      for(i=0;i<nplot3dinfo;i++){
        plot3ddata *plot3di;

        plot3di = plot3dinfo + i;
        if(strcmp(file_ptr,plot3di->file)==0){
          for(j=0;j<5;j++){
            plot3di->diff_valmin[j]=percentile_min[j];
            plot3di->diff_valmax[j]=percentile_max[j];
          }
          break;
        }
      }
      continue;
    }
  }

  fclose(stream);
  updateplot3dmenulabels();
  init_plot3dtimelist();
}

/* ------------------ parse_device_keyword ------------------------ */

void parse_device_keyword(FILE *stream, devicedata *devicei){
  float xyz[3]={0.0,0.0,0.0}, xyzn[3]={0.0,0.0,0.0};
  int state0=0;
  int nparams=0, nparams_textures=0;
  char *labelptr, *prop_id;
  char prop_buffer[255];
  char buffer[255],*buffer3;
  int i;
  char *tok1, *tok2, *tok3;

  devicei->type=DEVICE_DEVICE;
  fgets(buffer,255,stream);
  trim_commas(buffer);

  tok1=strtok(buffer,"%");
  tok1=trim_string(tok1);

  tok2=strtok(NULL,"%");
  tok2=trim_string(tok2);
  
  tok3=strtok(NULL,"%");
  tok3=trim_string(tok3);

  strcpy(devicei->quantity,"");
  if(tok2!=NULL){
    strcpy(devicei->quantity,tok2);
  }

  strcpy(devicei->label,tok1);
  devicei->object = get_SVOBJECT_type(tok1,missing_device);
  if(devicei->object==missing_device&&tok3!=NULL){
    devicei->object = get_SVOBJECT_type(tok3,missing_device);
  }
  devicei->params=NULL;
  devicei->times=NULL;
  devicei->vals=NULL;
  fgets(buffer,255,stream);
  trim_commas(buffer);

  sscanf(buffer,"%f %f %f %f %f %f %i %i %i",
    xyz,xyz+1,xyz+2,xyzn,xyzn+1,xyzn+2,&state0,&nparams,&nparams_textures);
  get_labels(buffer,-1,&prop_id,NULL,prop_buffer);
  devicei->prop=get_prop_id(prop_id);
  if(prop_id!=NULL&&devicei->prop!=NULL&&devicei->prop->smv_object!=NULL){
    devicei->object=devicei->prop->smv_object;
  }
  else{
    NewMemory((void **)&devicei->prop,sizeof(propdata));
    init_prop(devicei->prop,1,devicei->label);
    devicei->prop->smv_object=devicei->object;
    devicei->prop->smv_objects[0]=devicei->prop->smv_object;
  }
  if(nparams_textures<0)nparams_textures=0;
  if(nparams_textures>1)nparams_textures=1;
  devicei->ntextures=nparams_textures;
  if(nparams_textures>0){
     NewMemory((void **)&devicei->textureinfo,sizeof(texture));
  }
  else{
    devicei->textureinfo=NULL;
    devicei->texturefile=NULL;
  }

  labelptr=strchr(buffer,'%');
  if(labelptr!=NULL){
    trim(labelptr);
    if(strlen(labelptr)>1){
      labelptr++;
      labelptr=trim_front(labelptr);
      if(strlen(labelptr)==0)labelptr=NULL;
    }
    else{
      labelptr=NULL;
    }
  }

  if(nparams<=0){
    init_device(devicei,xyz,xyzn,state0,0,NULL,labelptr);
  }
  else{
    float *params,*pc;
    int nsize;

    nsize = 6*((nparams-1)/6+1);
    NewMemory((void **)&params,(nsize+devicei->ntextures)*sizeof(float));
    pc=params;
    for(i=0;i<nsize/6;i++){
      fgets(buffer,255,stream);
      trim_commas(buffer);

      sscanf(buffer,"%f %f %f %f %f %f",pc,pc+1,pc+2,pc+3,pc+4,pc+5);
      pc+=6;
    }
    init_device(devicei,xyz,xyzn,state0,nparams,params,labelptr);
  }
  get_elevaz(devicei->xyznorm,&devicei->dtheta,devicei->rotate_axis,NULL);
  if(nparams_textures>0){
    fgets(buffer,255,stream);
    trim_commas(buffer);
    trim(buffer);
    buffer3=trim_front(buffer);
    NewMemory((void **)&devicei->texturefile,strlen(buffer3)+1);
    strcpy(devicei->texturefile,buffer3);
  }
}

/* ------------------ get_inpf ------------------------ */

int get_inpf(char *file, char *file2){
  FILE *stream=NULL,*stream1=NULL,*stream2=NULL;
  char buffer[255],*bufferptr;
  int len;
  STRUCTSTAT statbuffer;

  if(file==NULL)return 1;
  stream1=fopen(file,"r");
  if(stream1==NULL)return 1;
  if(file2!=NULL){
    stream2=fopen(file2,"r");
    if(stream2==NULL){
      fclose(stream1);
      return 1;
    }
  }
  stream=stream1;
  for(;;){
    if(feof(stream)!=0){
      BREAK;
    }
    if(fgets(buffer,255,stream)==NULL){
      BREAK;
    }
    if(strncmp(buffer," ",1)==0)continue;
    if(match(buffer,"INPF") == 1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      bufferptr=trim_string(buffer);

      len=strlen(bufferptr);
      FREEMEMORY(fds_filein);
      if(NewMemory((void **)&fds_filein,(unsigned int)(len+1))==0)return 2;
      STRCPY(fds_filein,bufferptr);
      if(STAT(fds_filein,&statbuffer)!=0){
        FreeMemory(fds_filein);
        fds_filein=NULL;
      }

      if(chidfilebase==NULL){
        char *chidptr=NULL;
        char buffer_chid[1024];

        if(fds_filein!=NULL)chidptr=get_chid(fds_filein,buffer_chid);
        if(chidptr!=NULL){
          NewMemory((void **)&chidfilebase,(unsigned int)(strlen(chidptr)+1));
          STRCPY(chidfilebase,chidptr);
        }
      }
      if(chidfilebase!=NULL){
        NewMemory((void **)&hrr_csvfilename,(unsigned int)(strlen(chidfilebase)+8+1));
        STRCPY(hrr_csvfilename,chidfilebase);
        STRCAT(hrr_csvfilename,"_hrr.csv");
        if(STAT(hrr_csvfilename,&statbuffer)!=0){
          FREEMEMORY(hrr_csvfilename);
        }
        
        NewMemory((void **)&devc_csvfilename,(unsigned int)(strlen(chidfilebase)+9+1));
        STRCPY(devc_csvfilename,chidfilebase);
        STRCAT(devc_csvfilename,"_devc.csv");
        if(STAT(devc_csvfilename,&statbuffer)!=0){
          FREEMEMORY(devc_csvfilename);
        }

        NewMemory((void **)&exp_csvfilename,(unsigned int)(strlen(chidfilebase)+8+1));
        STRCPY(exp_csvfilename,chidfilebase);
        STRCAT(exp_csvfilename,"_exp.csv");
        if(STAT(exp_csvfilename,&statbuffer)!=0){
          FREEMEMORY(exp_csvfilename);
        }
      }
      continue;
    }
  }
  fclose(stream);
  return 0;
}

/* ------------------ init_textures ------------------------ */

void init_textures(void){
  // get texture filename from SURF and device info
  int i;

  printf("     Loading surface textures\n");
  ntextures = 0;
  for(i=0;i<nsurfinfo;i++){
    surfdata *surfi;
    texture *texti;
    int len;

    surfi = surfinfo + i;
    if(surfi->texturefile==NULL)continue;
    texti = textureinfo + ntextures;
    len = strlen(surfi->texturefile);
    NewMemory((void **)&texti->file,(len+1)*sizeof(char));
    strcpy(texti->file,surfi->texturefile);
    texti->loaded=0;
    texti->used=0;
    texti->display=0;
    ntextures++;
    surfi->textureinfo=textureinfo+ntextures-1;
  }

  for(i=0;i<ndevice_texture_list;i++){
    char *texturefile;
    texture *texti;
    int len;

    texturefile = device_texture_list[i];
    texti = textureinfo + ntextures;
    len = strlen(texturefile);
    NewMemory((void **)&texti->file,(len+1)*sizeof(char));
    device_texture_list_index[i]=ntextures;
    strcpy(texti->file,texturefile);
    texti->loaded=0;
    texti->used=0;
    texti->display=0;
    ntextures++;
  }

  // check to see if texture files exist .
  // If so, then convert to OpenGL format 
  
  for(i=0;i<ntextures;i++){
    unsigned char *floortex;
    int texwid, texht;
    texture *texti;
    int dup_texture;
    int j;

    texti = textureinfo + i;
    texti->loaded=0;
    if(texti->file==NULL){
      continue;
    }
    dup_texture=0;
    for(j=0;j<i;j++){
      texture *textj;

      textj = textureinfo + j;
      if(textj->loaded==0)continue;
      if(strcmp(texti->file,textj->file)==0){
        texti->name=textj->name;
        texti->loaded=1;
        dup_texture=1;
      }
    }
    if(dup_texture==1){
      continue;
    }
    if(use_graphics==1){
      int errorcode;
      char *filename;

      CheckMemory;
      filename=strrchr(texti->file,*dirseparator);
      if(filename!=NULL){
        filename++;
      }
      else{
        filename=texti->file;
      }
      printf("       Loading texture: %s",filename);
      glGenTextures(1,&texti->name);
      glBindTexture(GL_TEXTURE_2D,texti->name);
      floortex=readpicture(texti->file,&texwid,&texht,0);
      if(floortex==NULL){
         printf("%s",_(" - failed"));
         printf("\n");
         continue;
      }
      errorcode=gluBuild2DMipmaps(GL_TEXTURE_2D,4, texwid, texht, GL_RGBA, GL_UNSIGNED_BYTE, floortex);
      if(errorcode!=0){
        FREEMEMORY(floortex);
         printf("%s",_(" - failed"));
         printf("\n");
        continue;
      }
      FREEMEMORY(floortex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      texti->loaded=1;
      printf("%s",_(" - completed"));
      printf("\n");
    }
  }
  
  CheckMemory;
  if(ntextures==0){
    FREEMEMORY(textureinfo);
  }

  // define colobar textures

  printf("%s",_("       Loading colorbar texture"));

 // glActiveTexture(GL_TEXTURE0);
  glGenTextures(1,&texture_colorbar_id);
  glBindTexture(GL_TEXTURE_1D,texture_colorbar_id);
  glTexImage1D(GL_TEXTURE_1D,0,4,256,0,GL_RGBA,GL_FLOAT,rgb_full);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#ifdef pp_GPU
  if(gpuactive==1){
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  }
  else{
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  }
#else
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
#endif

  glGenTextures(1,&texture_slice_colorbar_id);
  glBindTexture(GL_TEXTURE_1D,texture_slice_colorbar_id);
  glTexImage1D(GL_TEXTURE_1D,0,4,256,0,GL_RGBA,GL_FLOAT,rgb_slice);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#ifdef pp_GPU
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
#else
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
#endif

  glGenTextures(1,&texture_patch_colorbar_id);
  glBindTexture(GL_TEXTURE_1D,texture_patch_colorbar_id);
  glTexImage1D(GL_TEXTURE_1D,0,4,256,0,GL_RGBA,GL_FLOAT,rgb_patch);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#ifdef pp_GPU
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
#else
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
#endif

  glGenTextures(1,&texture_plot3d_colorbar_id);
  glBindTexture(GL_TEXTURE_1D,texture_plot3d_colorbar_id);
  glTexImage1D(GL_TEXTURE_1D,0,4,256,0,GL_RGBA,GL_FLOAT,rgb_plot3d);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#ifdef pp_GPU
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
#else
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
#endif

  glGenTextures(1,&texture_iso_colorbar_id);
  glBindTexture(GL_TEXTURE_1D,texture_iso_colorbar_id);
  glTexImage1D(GL_TEXTURE_1D,0,4,256,0,GL_RGBA,GL_FLOAT,rgb_iso);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#ifdef pp_GPU
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
#else
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
#endif

  //glActiveTexture(GL_TEXTURE2);
  glGenTextures(1,&smokecolormap_id);
  glBindTexture(GL_TEXTURE_1D,smokecolormap_id);
  glTexImage1D(GL_TEXTURE_1D,0,4,256,0,GL_RGBA,GL_FLOAT,rgb_smokecolormap);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
#ifdef pp_GPU
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
#else
  glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
#endif
  CheckMemory;

  printf("%s"," - completed\n");
#ifdef pp_GPU
#ifdef pp_GPUDEPTH
  if(use_graphics==1){
    createDepthTexture();
  }
#endif
#endif

  if(autoterrain==1&&use_graphics==1){
    texture *tt;
    unsigned char *floortex;
    int texwid, texht;
    int errorcode;

    tt = terrain_texture;
    tt->loaded=0;
    tt->used=0;
    tt->display=0;
    printf("%s","     Loading terrain texture");

    glGenTextures(1,&tt->name);
    glBindTexture(GL_TEXTURE_2D,tt->name);
    floortex=NULL;
    errorcode=1;
    if(tt->file!=NULL){
      printf(": %s",tt->file);
      floortex=readpicture(tt->file,&texwid,&texht,0);
    }
    if(floortex!=NULL){
      errorcode=gluBuild2DMipmaps(GL_TEXTURE_2D,4, texwid, texht, GL_RGBA, GL_UNSIGNED_BYTE, floortex);
    }
    if(errorcode!=0){
      printf("%s"," - failed\n");
    }
    FREEMEMORY(floortex);
    if(errorcode==0){
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      tt->loaded=1;
      printf("%s"," - completed\n");
    }

  }
  printf("     Surface texture loading completed\n");
}

/* ------------------ update_bounds ------------------------ */

void update_bound_info(void){
  int i,n;

  if(nisoinfo>0){
    FREEMEMORY(isoindex);
    FREEMEMORY(isobounds);
    NewMemory((void*)&isoindex,nisoinfo*sizeof(int));
    NewMemory((void*)&isobounds,nisoinfo*sizeof(databounds));
    niso_bounds=0;
    for(i=0;i<nisoinfo;i++){
      isodata *isoi;

      isoi = isoinfo + i;
      if(isoi->dataflag==0)continue;
      isoi->firstshort=1;
      isoi->setvalmin=0;
      isoi->setvalmax=0;
      isoi->valmin=1.0;
      isoi->valmax=0.0;
      isoindex[niso_bounds]=i;
      isobounds[niso_bounds].datalabel=isoi->color_label.shortlabel;
      isobounds[niso_bounds].setvalmin=0;
      isobounds[niso_bounds].setvalmax=0;
      isobounds[niso_bounds].valmin=1.0;
      isobounds[niso_bounds].valmax=0.0;
      isobounds[niso_bounds].setchopmax=0;
      isobounds[niso_bounds].setchopmin=0;
      isobounds[niso_bounds].chopmax=0.0;
      isobounds[niso_bounds].chopmin=1.0;
      isobounds[niso_bounds].label=&isoi->color_label;
      niso_bounds++;
      for(n=0;n<i;n++){
        isodata *ison;

        ison = isoinfo + n;
        if(ison->dataflag==0)continue;
        if(strcmp(isoi->color_label.shortlabel,ison->color_label.shortlabel)==0){
          isoi->firstshort=0;
          niso_bounds--;
          break;
        }
      }
    }
  }

  if(nsliceinfo>0){
    FREEMEMORY(slicebounds);
    NewMemory((void*)&slicebounds,nsliceinfo*sizeof(databounds));
    nslice2=0;
    for(i=0;i<nsliceinfo;i++){
      slicedata *slicei;

      slicei = sliceinfo + i;
      slicei->firstshort=1;
      slicei->valmin=1.0;
      slicei->valmax=0.0;
      slicei->setvalmin=0;
      slicei->setvalmax=0;
      slicebounds[nslice2].datalabel=slicei->label.shortlabel;
      slicebounds[nslice2].setvalmin=0;
      slicebounds[nslice2].setvalmax=0;
      slicebounds[nslice2].valmin=1.0;
      slicebounds[nslice2].valmax=0.0;
      slicebounds[nslice2].chopmax=0.0;
      slicebounds[nslice2].chopmin=1.0;
      slicebounds[nslice2].setchopmax=0;
      slicebounds[nslice2].setchopmin=0;
#ifdef pp_SLICECONTOURS
      slicebounds[nslice2].line_contour_min=0.0;
      slicebounds[nslice2].line_contour_max=1.0;
      slicebounds[nslice2].line_contour_num=1;
#endif
      nslice2++;
      for(n=0;n<i;n++){
        slicedata *slicen;

        slicen = sliceinfo + n;
        if(strcmp(slicei->label.shortlabel,slicen->label.shortlabel)==0){
          slicei->firstshort=0;
          nslice2--;
          break;
        }
      }
    }
  }
  canshow_threshold=0;
  if(npatchinfo>0){
    npatch2=0;
    FREEMEMORY(patchlabellist);
    FREEMEMORY(patchlabellist_index);
    NewMemory((void **)&patchlabellist,npatchinfo*sizeof(char *));
    NewMemory((void **)&patchlabellist_index,npatchinfo*sizeof(int));
    for(i=0;i<npatchinfo;i++){
      patchinfo[i].firstshort=1;
      patchinfo[i].valmin=1.0;
      patchinfo[i].valmax=0.0;
      patchinfo[i].setvalmin=0;
      patchinfo[i].setvalmax=0;
      if(strncmp(patchinfo[i].label.shortlabel,"temp",4)==0||
         strncmp(patchinfo[i].label.shortlabel,"TEMP",4)==0){
        canshow_threshold=1;
      }
      patchlabellist[npatch2]=patchinfo[i].label.shortlabel;
      patchlabellist_index[npatch2]=i;
      npatch2++;
      for(n=0;n<i;n++){
        if(strcmp(patchinfo[i].label.shortlabel,patchinfo[n].label.shortlabel)==0){
          patchinfo[i].firstshort=0;
          npatch2--;
          break;
        }
      }
    }
  }
  updatechar();
}

/* ------------------ update_endian_info ------------------------ */

void update_endian_info(void){
  if(setendian==0){
    if(match(LESsystem,"AIX")==1||match(LESsystem,"SGI")==1||match(LESendian,"b")==1||match(LESendian,"B")==1){
      endian_data=1;
    }
    if(match(LESsystem,"DVF")==1||match(LESendian,"l")==1||match(LESendian,"L")==1){
      endian_data=0;
    }
    endian_smv = endian_data;
  }

#ifndef WIN32
  if(endian_smv!=getendian()){
    printf("*** Warning: Smokeview is running on a ");
    if(getendian()==1){
      printf(" little endian computer\n");
    }
    else{
      printf(" big endian computer\n");
    }
    printf("    but the data being visualized was generated on a ");
    if(endian_smv==1){
      printf(" little endian computer\n");
    }
    else{
      printf(" big endian computer\n");
    }
  }
#endif
}

/* ------------------ update_mesh_coords ------------------------ */

void update_mesh_coords(void){
  int nn, i,n;
  float dxsbar, dysbar, dzsbar;
  int factor;
  int igrid;

  if(setPDIM==0){
    for(nn=0;nn<=current_mesh->ibar;nn++){
      current_mesh->xplt[nn]=xbar0+(float)nn*(xbar-xbar0)/(float)current_mesh->ibar;
    }
    for(nn=0;nn<=current_mesh->jbar;nn++){
      current_mesh->yplt[nn]=ybar0+(float)nn*(ybar-ybar0)/(float)current_mesh->jbar;
    }
    for(nn=0;nn<=current_mesh->kbar;nn++){
      current_mesh->zplt[nn]=zbar0+(float)nn*(zbar-zbar0)/(float)current_mesh->kbar;
    }
  }

  /* define highlighted block */

  /* add in offsets */
  for(i=0;i<nmeshes;i++){
    mesh *meshi;
    int ii;

    meshi=meshinfo+i;
    meshi->xyz_bar[XXX] += meshi->offset[XXX];
    meshi->xyz_bar[YYY] += meshi->offset[YYY];
    meshi->xyz_bar[ZZZ] += meshi->offset[ZZZ];
    meshi->xyz_bar0[XXX] += meshi->offset[XXX];
    meshi->xyz_bar0[YYY] += meshi->offset[YYY];
    meshi->xyz_bar0[ZZZ] += meshi->offset[ZZZ];
    {
      float dx, dy, dz;

      dx = (meshi->xyz_bar[XXX] - meshi->xyz_bar0[XXX])/meshi->ibar;
      dy = (meshi->xyz_bar[YYY] - meshi->xyz_bar0[YYY])/meshi->jbar;
      dz = (meshi->xyz_bar[ZZZ] - meshi->xyz_bar0[ZZZ])/meshi->kbar;
      meshi->cellsize=sqrt(dx*dx+dy*dy+dz*dz);
    }
    for(ii=0;ii<meshi->ibar+1;ii++){
      meshi->xplt[ii] += meshi->offset[XXX];
    }
    for(ii=0;ii<meshi->jbar+1;ii++){
      meshi->yplt[ii] += meshi->offset[YYY];
    }
    for(ii=0;ii<meshi->kbar+1;ii++){
      meshi->zplt[ii] += meshi->offset[ZZZ];
    }
    meshi->xcen+=meshi->offset[XXX];
    meshi->ycen+=meshi->offset[YYY];
    meshi->zcen+=meshi->offset[ZZZ];
  }
  for(i=1;i<nmeshes;i++){
    mesh *meshi;

    meshi=meshinfo+i;
    if(meshi->xyz_bar0[ZZZ]!=meshinfo->xyz_bar0[ZZZ]){
      visFloor=0;
      updatefacelists=1;
      updatemenu=1;
      break;
    }
  }

  {
    mesh *meshi;

    meshi=meshinfo;

    xbar = meshi->xyz_bar[XXX];   
    ybar = meshi->xyz_bar[YYY];   
    zbar = meshi->xyz_bar[ZZZ];

    xbar0 = meshi->xyz_bar0[XXX]; 
    ybar0 = meshi->xyz_bar0[YYY]; 
    zbar0 = meshi->xyz_bar0[ZZZ];
  }

  ijkbarmax=meshinfo->ibar;
  for(i=0;i<nmeshes;i++){
    mesh *meshi;

    meshi=meshinfo+i;

    if(meshi->ibar>ijkbarmax)ijkbarmax=meshi->ibar;
    if(meshi->jbar>ijkbarmax)ijkbarmax=meshi->jbar;
    if(meshi->kbar>ijkbarmax)ijkbarmax=meshi->kbar;
  }

  for(i=1;i<nmeshes;i++){
    mesh *meshi;

    meshi=meshinfo+i;

    if(xbar <meshi->xyz_bar[XXX] )xbar =meshi->xyz_bar[XXX];
    if(ybar <meshi->xyz_bar[YYY] )ybar =meshi->xyz_bar[YYY];
    if(zbar <meshi->xyz_bar[ZZZ] )zbar =meshi->xyz_bar[ZZZ];
    if(xbar0>meshi->xyz_bar0[XXX])xbar0=meshi->xyz_bar0[XXX];
    if(ybar0>meshi->xyz_bar0[YYY])ybar0=meshi->xyz_bar0[YYY];
    if(zbar0>meshi->xyz_bar0[ZZZ])zbar0=meshi->xyz_bar0[ZZZ];
  }

  factor = 256*128;
  dxsbar = (xbar-xbar0)/factor;
  dysbar = (ybar-ybar0)/factor;
  dzsbar = (zbar-zbar0)/factor;

  for(nn=0;nn<factor;nn++){
    xplts[nn]=xbar0+((float)nn+0.5)*dxsbar;
  }
  for(nn=0;nn<factor;nn++){
    yplts[nn]=ybar0+((float)nn+0.5)*dysbar;
  }
  for(nn=0;nn<factor;nn++){
    zplts[nn]=zbar0+((float)nn+0.5)*dzsbar;
  }

  /* compute scaling factors */

  {
    float dxclip, dyclip, dzclip;

    dxclip = (xbar-xbar0)/1000.0;
    dyclip = (ybar-ybar0)/1000.0;
    dzclip = (zbar-zbar0)/1000.0;
    xclip_min = xbar0-dxclip;
    yclip_min = ybar0-dyclip;
    zclip_min = zbar0-dzclip;
    xclip_max = xbar+dxclip;
    yclip_max = ybar+dyclip;
    zclip_max = zbar+dzclip;
  }

  // compute scaling factor used in NORMALIXE_X, NORMALIZE_Y, NORMALIZE_Z macros

  xyzmaxdiff=xbar-xbar0;
  if(ybar-ybar0>xyzmaxdiff)xyzmaxdiff=ybar-ybar0;
  if(zbar-zbar0>xyzmaxdiff)xyzmaxdiff=zbar-zbar0;

  // normalize various coordinates.

  for(nn=0;nn<factor;nn++){
    xplts[nn]=NORMALIZE_X(xplts[nn]);
  }
  for(nn=0;nn<factor;nn++){
    yplts[nn]=NORMALIZE_Y(yplts[nn]);
  }
  for(nn=0;nn<factor;nn++){
    zplts[nn]=NORMALIZE_Z(zplts[nn]);
  }

  /* rescale both global and local xbar, ybar and zbar */

  xbar0ORIG = xbar0;
  ybar0ORIG = ybar0;
  zbar0ORIG = zbar0;
  xbarORIG = xbar;
  ybarORIG = ybar;
  zbarORIG = zbar;
  xbar = NORMALIZE_X(xbar);
  ybar = NORMALIZE_Y(ybar);
  zbar = NORMALIZE_Z(zbar);
  for(i=0;i<nmeshes;i++){
    mesh *meshi;

    meshi=meshinfo+i;
    /* compute a local scaling factor for each block */
    meshi->xyzmaxdiff=meshi->xyz_bar[XXX]-meshi->xyz_bar0[XXX];
    if(meshi->xyz_bar[YYY]-meshi->xyz_bar0[YYY]>meshi->xyzmaxdiff)meshi->xyzmaxdiff=meshi->xyz_bar[YYY]-meshi->xyz_bar0[YYY];
    if(meshi->xyz_bar[ZZZ]-meshi->xyz_bar0[ZZZ]>meshi->xyzmaxdiff)meshi->xyzmaxdiff=meshi->xyz_bar[ZZZ]-meshi->xyz_bar0[ZZZ];

    normalize_xyz(meshi->xyz_bar,meshi->xyz_bar);
    meshi->xcen = NORMALIZE_X(meshi->xcen);
    meshi->ycen = NORMALIZE_Y(meshi->ycen);
    meshi->zcen = NORMALIZE_Z(meshi->zcen);
  }

  for(i=0;i<noutlineinfo;i++){
    outline *outlinei;
    float *x1, *x2, *yy1, *yy2, *z1, *z2;
    int j;

    outlinei = outlineinfo + i;
    x1 = outlinei->x1;
    x2 = outlinei->x2;
    yy1 = outlinei->y1;
    yy2 = outlinei->y2;
    z1 = outlinei->z1;
    z2 = outlinei->z2;
    for(j=0;j<outlinei->nlines;j++){
      x1[j]=NORMALIZE_X(x1[j]);
      x2[j]=NORMALIZE_X(x2[j]);
      yy1[j]=NORMALIZE_Y(yy1[j]);
      yy2[j]=NORMALIZE_Y(yy2[j]);
      z1[j]=NORMALIZE_Z(z1[j]);
      z2[j]=NORMALIZE_Z(z2[j]);
    }
  }

  {
    mesh *meshi;
    meshi=meshinfo;
    veclength = meshi->xplt[1]-meshi->xplt[0];
  }
  for(i=0;i<nmeshes;i++){
    mesh *meshi;

    meshi=meshinfo+i;
    if(veclength>meshi->xplt[1]-meshi->xplt[0])veclength=meshi->xplt[1]-meshi->xplt[0];
    if(veclength>meshi->yplt[1]-meshi->yplt[0])veclength=meshi->yplt[1]-meshi->yplt[0];
    if(veclength>meshi->zplt[1]-meshi->zplt[0])veclength=meshi->zplt[1]-meshi->zplt[0];
  }
  veclength = veclength/xyzmaxdiff;
  veclength = 0.01;

  for(igrid=0;igrid<nmeshes;igrid++){
    mesh *meshi;
    float *face_centers;
    float *xplt_cen, *yplt_cen, *zplt_cen;
    int ibar, jbar, kbar;
    float *xplt_orig, *yplt_orig, *zplt_orig;
    float *xplt, *yplt, *zplt;
    int j,k;

    meshi=meshinfo+igrid;
    ibar=meshi->ibar; 
    jbar=meshi->jbar; 
    kbar=meshi->kbar;
    xplt_orig = meshi->xplt_orig;
    yplt_orig = meshi->yplt_orig;
    zplt_orig = meshi->zplt_orig;
    xplt = meshi->xplt;
    yplt = meshi->yplt;
    zplt = meshi->zplt;
    xplt_cen = meshi->xplt_cen;
    yplt_cen = meshi->yplt_cen;
    zplt_cen = meshi->zplt_cen;

    for(i=0;i<ibar+1;i++){
      xplt_orig[i]=xplt[i];
      xplt[i]=NORMALIZE_X(xplt[i]);
    }
    for(j=0;j<jbar+1;j++){
      yplt_orig[j]=yplt[j];
      yplt[j]=NORMALIZE_Y(yplt[j]);
    }
    for(k=0;k<kbar+1;k++){
      zplt_orig[k]=zplt[k];
      zplt[k]=NORMALIZE_Z(zplt[k]);
    }

    for(nn=0;nn<ibar;nn++){
      xplt_cen[nn]=(xplt[nn]+xplt[nn+1])/2.0;
    }
    for(nn=0;nn<jbar;nn++){
      yplt_cen[nn]=(yplt[nn]+yplt[nn+1])/2.0;
    }
    for(nn=0;nn<kbar;nn++){
      zplt_cen[nn]=(zplt[nn]+zplt[nn+1])/2.0;
    }

    meshi->boxoffset=-(zplt[1]-zplt[0])/10.0;
    meshi->boxmin[0]=xplt_orig[0];
    meshi->boxmin[1]=yplt_orig[0];
    meshi->boxmin[2]=zplt_orig[0];
    meshi->boxmax[0]=xplt_orig[ibar];
    meshi->boxmax[1]=yplt_orig[jbar];
    meshi->boxmax[2]=zplt_orig[kbar];
    normalize_xyz(meshi->boxmin_scaled,meshi->boxmin);
    normalize_xyz(meshi->boxmax_scaled,meshi->boxmax);
    meshi->x0 = xplt[0];
    meshi->x1 = xplt[ibar];
    meshi->y0 = yplt[0];
    meshi->y1 = yplt[jbar];
    meshi->z0 = zplt[0];
    meshi->z1 = zplt[kbar];

    face_centers = meshi->face_centers;
    for(j=0;j<6;j++){
      face_centers[0]=meshi->xcen;
      face_centers[1]=meshi->ycen;
      face_centers[2]=meshi->zcen;
      face_centers+=3;
    }
    face_centers = meshi->face_centers;
    face_centers[0]=meshi->boxmin_scaled[0];
    face_centers[3]=meshi->boxmax_scaled[0];
    face_centers[7]=meshi->boxmin_scaled[1];
    face_centers[10]=meshi->boxmax_scaled[1];
    face_centers[14]=meshi->boxmin_scaled[2];
    face_centers[17]=meshi->boxmax_scaled[2];
  }

  nsmoothblocks=0;
  ntransparentblocks=0;
  ntransparentvents=0;
  nopenvents=0;
  nopenvents_nonoutline=0;
  ndummyvents=0;
  for(igrid=0;igrid<nmeshes;igrid++){
    mesh *meshi;

    meshi=meshinfo+igrid;
    for(i=0;i<meshi->nbptrs;i++){
      blockagedata *bc;

      bc=meshi->blockageinfoptrs[i];
      if(bc->type==BLOCK_smooth)nsmoothblocks++;
      if(bc->color[3]<0.99)ntransparentblocks++;
    }
    for(i=0;i<meshi->nvents;i++){
      ventdata *vi;

      vi = meshi->ventinfo + i;
      if(vi->isOpenvent==1){
        nopenvents++;
        if(vi->type!=BLOCK_OUTLINE)nopenvents_nonoutline++;
      }
      if(vi->dummy==1)ndummyvents++;
      if(vi->color[3]<0.99)ntransparentvents++;
    }
  }

  for(igrid=0;igrid<nmeshes;igrid++){
    mesh *meshi;

    meshi=meshinfo+igrid;
    for(i=0;i<meshi->nbptrs;i++){
      blockagedata *bc;

      bc=meshi->blockageinfoptrs[i];
      bc->xmin += meshi->offset[XXX];
      bc->xmax += meshi->offset[XXX];
      bc->ymin += meshi->offset[YYY];
      bc->ymax += meshi->offset[YYY];
      bc->zmin += meshi->offset[ZZZ];
      bc->zmax += meshi->offset[ZZZ];
      bc->xmin = NORMALIZE_X(bc->xmin);
      bc->xmax = NORMALIZE_X(bc->xmax);
      bc->ymin = NORMALIZE_Y(bc->ymin);
      bc->ymax = NORMALIZE_Y(bc->ymax);
      bc->zmin = NORMALIZE_Z(bc->zmin);
      bc->zmax = NORMALIZE_Z(bc->zmax);
      bc->xyzORIG[0]=bc->xmin;
      bc->xyzORIG[1]=bc->xmax;
      bc->xyzORIG[2]=bc->ymin;
      bc->xyzORIG[3]=bc->ymax;
      bc->xyzORIG[4]=bc->zmin;
      bc->xyzORIG[5]=bc->zmax;
      bc->ijkORIG[0]=bc->ijk[0];
      bc->ijkORIG[1]=bc->ijk[1];
      bc->ijkORIG[2]=bc->ijk[2];
      bc->ijkORIG[3]=bc->ijk[3];
      bc->ijkORIG[4]=bc->ijk[4];
      bc->ijkORIG[5]=bc->ijk[5];
    }
    for(i=0;i<meshi->nvents+12;i++){
      ventdata *vi;

      vi=meshi->ventinfo+i;
      vi->xmin = NORMALIZE_X(vi->xmin);
      vi->xmax = NORMALIZE_X(vi->xmax);
      vi->ymin = NORMALIZE_Y(vi->ymin);
      vi->ymax = NORMALIZE_Y(vi->ymax);
      vi->zmin = NORMALIZE_Z(vi->zmin);
      vi->zmax = NORMALIZE_Z(vi->zmax);
    }
  }
  for(igrid=0;igrid<nmeshes;igrid++){
    mesh *meshi;

    meshi=meshinfo+igrid;
    for(i=0;i<meshi->nbptrs;i++){
      blockagedata *bc;

      bc=meshi->blockageinfoptrs[i];
      backup_blockage(bc);
    }
  }

  for(i=0;i<ncadgeom;i++){
    cadgeom *cd;
    int j;

    cd=cadgeominfo+i;
    for(j=0;j<cd->nquads;j++){
      int k;
      cadquad *quadi;

      quadi = cd->quad+j;
      for(k=0;k<4;k++){
        normalize_xyz(quadi->xyzpoints+3*k,quadi->xyzpoints+3*k);
      }
      if(cd->version==2&&quadi->cadlookq->textureinfo.loaded==1){
        update_cadtextcoords(quadi);
      }
    }
  }
  for(n=0;n<nrooms;n++){
    roomdata *roomi;

    roomi = roominfo + n;
    roomi->x0=NORMALIZE_X(roomi->x0);
    roomi->y0=NORMALIZE_Y(roomi->y0);
    roomi->z0=NORMALIZE_Z(roomi->z0);
    roomi->x1=NORMALIZE_X(roomi->x1);
    roomi->y1=NORMALIZE_Y(roomi->y1);
    roomi->z1=NORMALIZE_Z(roomi->z1);
    roomi->dx=roomi->dx/xyzmaxdiff;
    roomi->dy=roomi->dy/xyzmaxdiff;
    roomi->dz=roomi->dz/xyzmaxdiff;
  }
  for(n=0;n<nfires;n++){
    firedata *firen;

    firen = fireinfo + n;
    firen->absx=NORMALIZE_X(firen->absx);
    firen->absy=NORMALIZE_Y(firen->absy);
    firen->absz=NORMALIZE_Z(firen->absz);
    firen->dz=firen->dz/xyzmaxdiff;
  }
  for(n=0;n<nzvents;n++){
    zvent *zvi;

    zvi = zventinfo + n;

    switch (zvi->dir){
    case 1:
    case 3:
      zvi->x1 = NORMALIZE_X(zvi->x1);
      zvi->x2 = NORMALIZE_X(zvi->x2);
      zvi->z1 = NORMALIZE_Z(zvi->z1);
      zvi->z2 = NORMALIZE_Z(zvi->z2);
      zvi->yy = NORMALIZE_Y(zvi->yy);
      break;
    case 2:
    case 4:
      zvi->x1 = NORMALIZE_Y(zvi->x1);
      zvi->x2 = NORMALIZE_Y(zvi->x2);
      zvi->z1 = NORMALIZE_Z(zvi->z1);
      zvi->z2 = NORMALIZE_Z(zvi->z2);
      zvi->yy = NORMALIZE_X(zvi->yy);
      break;
    case 5:
    case 6:
      zvi->x1 = NORMALIZE_X(zvi->x1);
      zvi->x2 = NORMALIZE_X(zvi->x2);
      zvi->y1 = NORMALIZE_Y(zvi->y1);
      zvi->y2 = NORMALIZE_Y(zvi->y2);
      zvi->zz = NORMALIZE_Z(zvi->zz);
      break;
    default:
      ASSERT(FFALSE);
      break;
    }
  }

  /* if we have to create smooth block structures do it now,
     otherwise postpone job until we view all blockages as smooth */

//  the code below was moved to after readini in startup (so that sb_atstart will be defined)
//  if(sb_atstart==1){
//    smooth_blockages();
//  }

  for(i=0;i<nmeshes;i++){
    mesh *meshi;
    float *xspr, *yspr, *zspr;
    float *xsprplot, *ysprplot, *zsprplot;
    float *xheat, *yheat, *zheat;
    float *xheatplot, *yheatplot, *zheatplot;
    float *offset;

    meshi=meshinfo + i;
    offset = meshi->offset;
    xsprplot = meshi->xsprplot;
    ysprplot = meshi->ysprplot;
    zsprplot = meshi->zsprplot;
    xspr = meshi->xspr;
    yspr = meshi->yspr;
    zspr = meshi->zspr;
    xheatplot = meshi->xheatplot;
    yheatplot = meshi->yheatplot;
    zheatplot = meshi->zheatplot;
    xheat = meshi->xheat;
    yheat = meshi->yheat;
    zheat = meshi->zheat;
    for(n=0;n<meshi->nspr;n++){
      xsprplot[n]=NORMALIZE_X(offset[XXX]+xspr[n]);
      ysprplot[n]=NORMALIZE_Y(offset[YYY]+yspr[n]);
      zsprplot[n]=NORMALIZE_Z(offset[ZZZ]+zspr[n]);
    }
    for(n=0;n<meshi->nheat;n++){
      xheatplot[n]=NORMALIZE_X(offset[XXX]+xheat[n]);
      yheatplot[n]=NORMALIZE_Y(offset[YYY]+yheat[n]);
      zheatplot[n]=NORMALIZE_Z(offset[ZZZ]+zheat[n]);
    }
    for(n=0;n<meshi->nvents+12;n++){
      ventdata *vi;

      vi = meshi->ventinfo+n;
      vi->xvent1plot=NORMALIZE_X(offset[XXX]+vi->xvent1);
      vi->xvent2plot=NORMALIZE_X(offset[XXX]+vi->xvent2);
      vi->yvent1plot=NORMALIZE_Y(offset[YYY]+vi->yvent1);
      vi->yvent2plot=NORMALIZE_Y(offset[YYY]+vi->yvent2);
      vi->zvent1plot=NORMALIZE_Z(offset[ZZZ]+vi->zvent1);
      vi->zvent2plot=NORMALIZE_Z(offset[ZZZ]+vi->zvent2);
    }
  }

  for(i=0;i<nmeshes;i++){
    mesh *meshi;

    meshi=meshinfo + i;
    meshi->vent_offset[XXX] = ventoffset_factor*(meshi->xplt[1]-meshi->xplt[0]);
    meshi->vent_offset[YYY] = ventoffset_factor*(meshi->yplt[1]-meshi->yplt[0]);
    meshi->vent_offset[ZZZ] = ventoffset_factor*(meshi->zplt[1]-meshi->zplt[0]);
  }
}

/* ------------------ readsmv ------------------------ */

int readsmv(char *file, char *file2){

/* read the .smv file */

  int unit_start=20;
  devicedata *devicecopy;
  int do_pass4=0;
  int roomdefined=0;
  float *x1, *x2, *yy1, *yy2, *z1, *z2;
  float temp_ignition, emis, t_width, t_height;
  float s_color[4];
  int *ijk;
  int s_type;
  int errorcode;
  int noGRIDpresent=1,startpass;
  int nslicefiles=0;

  int ipart=0, islice=0, islicecount=1, ipatch=0, iroom=0,izone_local=0,ifire=0,iiso=0;
  int ismoke3d=0,ismoke3dcount=1;
  int  itarg=0;
  int setGRID=0;
  int idummy;
  int nvents;
  int tempval;
  int iv1, iv2;
  int jv1, jv2;
  int kv1, kv2;
  int colorindex, blocktype;
  int ventindex,venttype;
  int roomnumber;
  float width,ventoffset,bottom,top;
  int igrid;
  int ioffset;
  float *xplttemp,*yplttemp,*zplttemp;
  partdata *parti;
  int itrnx, itrny, itrnz, ipdim, iobst, ivent;
  int ibartemp=2, jbartemp=2, kbartemp=2;
  size_t len;
  int  n;
  FILE *stream=NULL,*stream1=NULL,*stream2=NULL;
  FILE *ENDIANfile;
  char buffer[255],buffer2[255],*bufferptr;
  char *buffer3;
  int blocknumber;
  float *xsprcopy, *ysprcopy, *zsprcopy;
  float *xheatcopy, *yheatcopy, *zheatcopy;
  int nn;
  int i, j, k;
  STRUCTSTAT statbuffer,statbuffer2;
  int version;
  
  int nn_smoke3d=0;
  int nn_patch=0;
  int nn_iso=0;
  int nn_part=0;
  int nn_slice=0;

  npropinfo=1; // the 0'th prop is the default human property
  navatar_colors=0;
  FREEMEMORY(avatar_colors);

  FREEMEMORY(treeinfo);
  ntreeinfo=0;
  for(i=0;i<nterraininfo;i++){
    terraindata *terri;

    terri = terraininfo + i;
    FREEMEMORY(terri->x);
    FREEMEMORY(terri->y);
    FREEMEMORY(terri->zcell);
    FREEMEMORY(terri->znode);
//    FREEMEMORY(terri->znormal);
  }
  FREEMEMORY(terraininfo);
  nterraininfo=0;
  niso_compressed=0;
  if(sphereinfo==NULL){
    NewMemory((void **)&sphereinfo,sizeof(spherepoints));
    initspherepoints(sphereinfo,14);
  }
  if(wui_sphereinfo==NULL){
    NewMemory((void **)&wui_sphereinfo,sizeof(spherepoints));
    initspherepoints(wui_sphereinfo,14);
  }

  ntotal_blockages=0;
  ntotal_smooth_blockages=0;

  if(ncsvinfo>0){
    csvdata *csvi;

    for(i=0;i<ncsvinfo;i++){
      csvi = csvinfo + i;
      FREEMEMORY(csvi->file);
    }
    FREEMEMORY(csvinfo);
  }
  ncsvinfo=0;

  if(ngeominfo>0){
    for(i=0;i<ngeominfo;i++){
      geomdata *geomi;

      geomi = geominfo + i;
      FREEMEMORY(geomi->file);
    }
    FREEMEMORY(geominfo);
    ngeominfo=0;
  }

  FREEMEMORY(tickinfo);
  nticks=0;
  ntickssmv=0;

  FREEMEMORY(labelinfo);
  nlabels=0;
  nlabelssmv=0;

  FREEMEMORY(camera_external);
  if(file!=NULL)NewMemory((void **)&camera_external,sizeof(camera));

  FREEMEMORY(camera_external_save);
  if(file!=NULL)NewMemory((void **)&camera_external_save,sizeof(camera));

  FREEMEMORY(camera_ini);
  if(file!=NULL){
    NewMemory((void **)&camera_ini,sizeof(camera));
    camera_ini->defined=0;
  }

  FREEMEMORY(camera_current);
  if(file!=NULL)NewMemory((void **)&camera_current,sizeof(camera));

  FREEMEMORY(camera_internal);
  if(file!=NULL)NewMemory((void **)&camera_internal,sizeof(camera));

  FREEMEMORY(camera_save);
  if(file!=NULL)NewMemory((void **)&camera_save,sizeof(camera));

  FREEMEMORY(camera_last);
  if(file!=NULL)NewMemory((void **)&camera_last,sizeof(camera));
 
  updatefaces=1;
  STRCPY(TITLE1,"");
  STRCPY(TITLE2,"");
  nfires=0;
  nrooms=0;

  if(file!=NULL){
    initsurface(&sdefault);
    NewMemory((void **)&sdefault.surfacelabel,(5+1));
    strcpy(sdefault.surfacelabel,"INERT");

    initventsurface(&v_surfacedefault);
    NewMemory((void **)&v_surfacedefault.surfacelabel,(4+1));
    strcpy(v_surfacedefault.surfacelabel,"VENT");
  
    initsurface(&e_surfacedefault);
    NewMemory((void **)&e_surfacedefault.surfacelabel,(8+1));
    strcpy(e_surfacedefault.surfacelabel,"EXTERIOR");
    e_surfacedefault.color=mat_ambient2;
  }

  // free memory for particle class

  if(partclassinfo!=NULL){
    for(i=0;i<npartclassinfo+1;i++){
      part5class *partclassi;

      partclassi = partclassinfo + i;
      FREEMEMORY(partclassi->name);
      if(partclassi->ntypes>0){
        for(j=0;j<partclassi->ntypes;j++){
          flowlabels *labelj;
          
          labelj = partclassi->labels+j;
          freelabels(labelj);
        }
        FREEMEMORY(partclassi->labels);
        partclassi->ntypes=0;
      }
    }
    FREEMEMORY(partclassinfo);
  }
  npartclassinfo=0;

  freeall_objects();
  if(ndeviceinfo>0){
    for(i=0;i<ndeviceinfo;i++){
    }
    FREEMEMORY(deviceinfo);
    ndeviceinfo=0;
  }

  // read in device (.svo) definitions

  init_object_defs();
  {
    int return_code;
    
  // get input file name
  
    return_code=get_inpf(file,file2);
    if(return_code!=0)return return_code;
  }

  if(noutlineinfo>0){
    for(i=0;i<noutlineinfo;i++){
      outline *outlinei;

      outlinei = outlineinfo + i;
      FREEMEMORY(outlinei->x1);
      FREEMEMORY(outlinei->y1);
      FREEMEMORY(outlinei->z1);
      FREEMEMORY(outlinei->x2);
      FREEMEMORY(outlinei->y2);
      FREEMEMORY(outlinei->z2);
    }
    FREEMEMORY(outlineinfo);
    noutlineinfo=0;
  }

  if(nzoneinfo>0){
    for(i=0;i<nzoneinfo;i++){
      zonedata *zonei;

      zonei = zoneinfo + i;
      for(n=0;n<4;n++){
        freelabels(&zonei->label[n]);
      }
      FREEMEMORY(zonei->file);
    }
    FREEMEMORY(zoneinfo);
  }
  nzoneinfo=0;

  if(nsmoke3dinfo>0){
    {
      smoke3ddata *smoke3di;

      for(i=0;i<nsmoke3dinfo;i++){
        smoke3di = smoke3dinfo + i;
        freesmoke3d(smoke3di);
        FREEMEMORY(smoke3di->comp_file);
        FREEMEMORY(smoke3di->reg_file);
      }
      FREEMEMORY(smoke3dinfo);
      nsmoke3dinfo=0;
    }
  }

  if(npartinfo>0){
    for(i=0;i<npartinfo;i++){
      freelabels(&partinfo[i].label);
      FREEMEMORY(partinfo[i].partclassptr);
      FREEMEMORY(partinfo[i].reg_file);
      FREEMEMORY(partinfo[i].comp_file);
      FREEMEMORY(partinfo[i].size_file);
    }
    FREEMEMORY(partinfo);
  }
  npartinfo=0;

  ntarginfo=0;

  FREEMEMORY(surfinfo);


  //*** free slice data

  if(nsliceinfo>0){
    for(i=0;i<nsliceinfo;i++){
      slicedata *sd;
      sd = sliceinfo + i;
      freelabels(&sliceinfo[i].label);
      FREEMEMORY(sd->reg_file);
      FREEMEMORY(sd->comp_file);
      FREEMEMORY(sd->size_file);
    }
    FREEMEMORY(sliceorderindex);
    for(i=0;i<nmultislices;i++){
      multislicedata *mslicei;

      mslicei = multisliceinfo + i;
      FREEMEMORY(mslicei->islices);
    }
    FREEMEMORY(multisliceinfo);
    nmultislices=0;
    FREEMEMORY(sliceinfo);
  }
  nsliceinfo=0;

  //*** free multi-vector slice data

  if(nvslice>0){
    FREEMEMORY(vsliceorderindex);
    for(i=0;i<nmultivslices;i++){
      multivslicedata *mvslicei;

      mvslicei = multivsliceinfo + i;
      FREEMEMORY(mvslicei->ivslices);
    }
    FREEMEMORY(multivsliceinfo);
    nmultivslices=0;
  }

  if(npatchinfo>0){
    for(i=0;i<npatchinfo;i++){
      freelabels(&patchinfo[i].label);
      FREEMEMORY(patchinfo[i].reg_file);
      FREEMEMORY(patchinfo[i].comp_file);
      FREEMEMORY(patchinfo[i].size_file);
    }
    FREEMEMORY(patchinfo);
  }
  npatchinfo=0;
  
  if(nisoinfo>0){
    for(i=0;i<nisoinfo;i++){
      freelabels(&isoinfo[i].surface_label);
      FREEMEMORY(isoinfo[i].file);
    }
    FREEMEMORY(isoinfo);
  }
  nisoinfo=0;

  freecadinfo();

  updateindexcolors=0;
  ntrnx=0;
  ntrny=0;
  ntrnz=0;
  nmeshes=0;
  npdim=0;
  nvent=0;
  nobst=0;
  noffset=0;
  nsurfinfo=0;
  nvent_transparent=0;

  nvents=0; 
  setPDIM=0;
  endian_smv = getendian();
  endian_native = getendian();
  endian_data=endian_native;
  FREEMEMORY(LESsystem);
  FREEMEMORY(LESendian);

  FREEMEMORY(databasefilename);

  FREEMEMORY(targinfo);

  FREEMEMORY(vsliceinfo);
  FREEMEMORY(sliceinfo);
  FREEMEMORY(slicetypes);
  FREEMEMORY(vslicetypes);

  FREEMEMORY(plot3dinfo);
  FREEMEMORY(patchinfo);
  FREEMEMORY(patchtypes);
  FREEMEMORY(isoinfo);
  FREEMEMORY(isotypes);
  FREEMEMORY(roominfo);
  FREEMEMORY(fireinfo);
  FREEMEMORY(zoneinfo);
  FREEMEMORY(zventinfo);

  FREEMEMORY(textureinfo);
  FREEMEMORY(surfinfo);
  FREEMEMORY(terrain_texture);

  if(cadgeominfo!=NULL)freecadinfo();

  if(file==NULL){
    initvars();
    return -1;  // finished  unloading memory from previous case
  }

  if(NewMemory((void **)&LESsystem,4)==0)return 2;
  STRCPY(LESsystem,"");
  if(NewMemory((void **)&LESendian,4)==0)return 2;
  STRCPY(LESendian,"");

  stream1=fopen(file,"r");
  if(stream1==NULL)return 1;
  if(file2!=NULL){
    stream2=fopen(file2,"r");
    if(stream2==NULL){
      fclose(stream1);
      return 1;
    }
  }
  stream=stream1;

  smv_modtime=file_modtime(file);
  
  printf(_("processing smokeview file: %s\n"),file);

/* 
   ************************************************************************
   ************************ start of pass 1 ********************************* 
   ************************************************************************
 */

  nvents=0; 
  igrid=0; 
  ioffset=0;
  ntc_total=0;
  nspr_total=0;
  nheat_total=0;
  printf("%s",_("   pass 1 started"));
  printf("\n");
  for(;;){
    if(feof(stream)!=0){
      BREAK;
    }
    if(fgets(buffer,255,stream)==NULL){
      BREAK;
    }
    trim(buffer);
    if(strncmp(buffer," ",1)==0||buffer[0]==0)continue;

    /* 
      The keywords TRNX, TRNY, TRNZ, GRID, PDIM, OBST and VENT are not required 
      BUT if any one these keywords are present then the number of each MUST be equal 
    */


    if(match(buffer,"CSVF") == 1){
      int nfiles;
      char *file_ptr,*type_ptr;

      fgets(buffer,255,stream);
      trim(buffer);
      type_ptr=trim_front(buffer);

      fgets(buffer2,255,stream);
      trim(buffer2);
      file_ptr=trim_front(buffer2);
      nfiles=1;
      if(strcmp(type_ptr,"hrr")==0){
        if(file_exists(file_ptr)==0)nfiles=0;
      }
      else if(strcmp(type_ptr,"devc")==0){
        if(file_exists(file_ptr)==0)nfiles=0;
      }
      else if(strcmp(type_ptr,"ext")==0){
        if(strchr(file_ptr,'*')==NULL){
          if(file_exists(file_ptr)==0)nfiles=0;
        }
        else{
          nfiles = get_nfilelist(".",file_ptr);
        }
      }
      else{
        nfiles=0;
      }
      ncsvinfo+=nfiles;
      continue;
    }
    if(match(buffer,"GEOM") == 1){
      ngeominfo++;
      continue;
    }
    if(match(buffer,"PROP") == 1){
      npropinfo++;
      continue;
    }
    if(match(buffer,"SMOKEDIFF") == 1){
      smokediff=1;
      continue;
    }
    if(match(buffer,"ALBEDO") == 1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f",&smoke_shade);
      smoke_shade=0.0;
      continue;
    }
    if(match(buffer,"AVATAR_COLOR") == 1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&navatar_colors);
      if(navatar_colors<0)navatar_colors=0;
      if(navatar_colors>0){
        float *acolor;

        NewMemory((void **)&avatar_colors,3*navatar_colors*sizeof(float));
        acolor=avatar_colors;
        for(i=0;i<navatar_colors;i++){
          int irgb[3];
          fgets(buffer,255,stream);
          irgb[0]=0;
          irgb[1]=0;
          irgb[2]=0;
          sscanf(buffer,"%i %i %i",irgb,irgb+1,irgb+2);
          acolor[0]=(float)irgb[0]/255.0;
          acolor[1]=(float)irgb[1]/255.0;
          acolor[2]=(float)irgb[2]/255.0;
          acolor+=3;
        }
      }
      continue;
    }
    if(match(buffer,"TERRAIN") == 1){
      nterraininfo++;
      continue;
    }
    if(match(buffer,"CLASS_OF_PARTICLES") == 1||
       match(buffer,"CLASS_OF_HUMANS") == 1){
      npartclassinfo++;
      continue;
    }
    if(match(buffer,"AUTOTERRAIN") == 1){
      int len_buffer;
      texture *tt;
      char *buff2;

      NewMemory((void **)&terrain_texture,sizeof(texture));
      tt = terrain_texture;
      autoterrain=1;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visTerrainType);
    //  if(visTerrain!=1)visTerrain=0;
  
      fgets(buffer,255,stream);
      buff2 = trim_front(buffer);
      trim(buff2);
      len_buffer = strlen(buff2);

      NewMemory((void **)&tt->file,(len_buffer+1)*sizeof(char));
      strcpy(tt->file,buff2);

      continue;
    }
    if(
      (match(buffer,"DEVICE") == 1)&&
      (match(buffer,"DEVICE_ACT") != 1)
      ){
      fgets(buffer,255,stream);
      fgets(buffer,255,stream);
      ndeviceinfo++;
      continue;
    }
    if(
        match(buffer,"SPRK") == 1||
       match(buffer,"HEAT") == 1||
       match(buffer,"SMOD") == 1||
       match(buffer,"THCP") == 1
    ){
      int local_ntc;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&local_ntc);
      if(local_ntc<0)local_ntc=0;
      ndeviceinfo+=local_ntc;
      for(i=0;i<local_ntc;i++){
        fgets(buffer,255,stream);
      }
      continue;
    }
    if(match(buffer,"REVISION")==1){
      revision_fds=-1;
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      sscanf(buffer,"%i",&revision_fds);
      if(revision_fds<0)revision_fds=-1;
      continue;
    }
    if(match(buffer,"TOFFSET")==1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      sscanf(buffer,"%f %f %f",texture_origin,texture_origin+1,texture_origin+2);
      continue;
    }

    if(match(buffer,"USETEXTURES") == 1){
      usetextures=1;
      continue;
    }

    if(match(buffer,"CADTEXTUREPATH") == 1||
       match(buffer,"TEXTUREDIR") == 1){
         if(fgets(buffer,255,stream)==NULL){
           BREAK;
         }
      trim(buffer);
      {
        size_t texturedirlen;

        texturedirlen=strlen(trim_front(buffer));
        if(texturedirlen>0){
          FREEMEMORY(texturedir);
          NewMemory( (void **)&texturedir,texturedirlen+1);
          strcpy(texturedir,trim_front(buffer));
        }
      }
      continue;
    }

    if(match(buffer,"VIEWTIMES") == 1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      sscanf(buffer,"%f %f %i",&view_tstart,&view_tstop,&view_ntimes);
      if(view_ntimes<2)view_ntimes=2;
      ReallocTourMemory();
      continue;
    }
    if(match(buffer,"OUTLINE") == 1){
      noutlineinfo++;
      continue;
    }
    if(match(buffer,"TICKS") == 1){
      nticks++;
      ntickssmv++;
      continue;
    }
    if(match(buffer,"LABEL") == 1){
      nlabels++;
      nlabelssmv++;
      continue;
    }
    if(match(buffer,"TRNX") == 1){
      ntrnx++;
      continue;
    }
    if(match(buffer,"TRNY") == 1){
      ntrny++;
      continue;
    }
    if(match(buffer,"TRNZ") == 1){
      ntrnz++;
      continue;
    }
    if(match(buffer,"SURFACE") ==1){
      nsurfinfo++;
      continue;
    }
    if(match(buffer,"GRID") == 1){
      noGRIDpresent=0;
      nmeshes++;
      continue;
    }
    if(match(buffer,"OFFSET") == 1){
      noffset++;
      continue;
    }
    if(match(buffer,"PDIM") == 1){
      npdim++;
      setPDIM=1;
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f %f %f %f",&xbar0,&xbar,&ybar0,&ybar,&zbar0,&zbar);
      continue;
    }
    if(match(buffer,"OBST") == 1){
      nobst++;
      continue;
    }
    if(match(buffer,"CADGEOM") == 1){
      ncadgeom++;
      continue;
    }
    if(match(buffer,"VENT") == 1){
      nvent++;
      continue;
    }
    if(match(buffer,"PART") == 1||match(buffer,"EVAC")==1
      ||match(buffer,"PRT5")==1||match(buffer,"EVA5")==1
      ){
      npartinfo++;
      continue;
    }
    if( (match(buffer,"SLCF") == 1)||
        (match(buffer,"SLCC") == 1)||
        (match(buffer,"SLFL") == 1)||
        (match(buffer,"SLCT") == 1)
      ){
      nsliceinfo++;
      nslicefiles=nsliceinfo;
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      continue;
    }
    if(
      match(buffer,"SMOKE3D") == 1||
      match(buffer,"VSMOKE3D") == 1||
      match(buffer,"SMOKF3D") == 1||
      match(buffer,"VSMOKF3D") == 1
      ){
      nsmoke3dinfo++;
      continue;
    }
    if(
      match(buffer,"MINMAXBNDF") == 1||
      match(buffer,"MINMAXPL3D") == 1||
      match(buffer,"MINMAXSLCF") == 1
      ){
      do_pass4=1;
      continue;
    }
    if(match(buffer,"BNDF") == 1|| match(buffer,"BNDC") == 1||match(buffer,"BNDE") == 1){
      npatchinfo++;
      continue;
    }
    if(match(buffer,"ISOF") == 1||match(buffer,"TISOF")==1||match(buffer,"ISOG") == 1){
      nisoinfo++;
      continue;
    }
    if(match(buffer,"ROOM") == 1){
      isZoneFireModel=1;
      nrooms++;
      continue;
    }
    if(match(buffer,"FIRE") == 1){
      nfires++;
      continue;
    }
    if(match(buffer,"ZONE") == 1){
      nzoneinfo++;
      continue;
    }
    if(
      match(buffer,"TARG") ==1||
      match(buffer,"FTARG")==1
      ){
      ntarginfo++;
      continue;
    }
    if(match(buffer,"VENTGEOM") == 1||match(buffer,"VFLOWGEOM")==1){
      nzvents++;
      continue;
    }

  }

/* 
   ************************************************************************
   ************************ end of pass 1 ********************************* 
   ************************************************************************
 */

 if(ncsvinfo>0){
   NewMemory((void **)&csvinfo,ncsvinfo*sizeof(csvdata));
   ncsvinfo=0;
 }
 if(ngeominfo>0){
   NewMemory((void **)&geominfo,ngeominfo*sizeof(geomdata));
   ngeominfo=0;
 }
 if(npropinfo>0){
   NewMemory((void **)&propinfo,npropinfo*sizeof(propdata));
   npropinfo=1; // the 0'th prop is the default human property
 }
 if(nterraininfo>0){
   NewMemory((void **)&terraininfo,nterraininfo*sizeof(terraindata));
   nterraininfo=0;
 }
 if(npartclassinfo>=0){
   float rgb_class[4];
   part5class *partclassi;


   NewMemory((void **)&partclassinfo,(npartclassinfo+1)*sizeof(part5class));

   // define a dummy class

   partclassi = partclassinfo + npartclassinfo;
   strcpy(buffer,"Default");
   trim(buffer);
   len=strlen(buffer);
   partclassi->name=NULL;
   if(len>0){
     NewMemory((void **)&partclassi->name,len+1);
     STRCPY(partclassi->name,trim_front(buffer));
   }

   rgb_class[0]=1.0;
   rgb_class[1]=0.0;
   rgb_class[2]=0.0;
   rgb_class[3]=1.0;
   partclassi->rgb=getcolorptr(rgb_class);

   partclassi->ntypes=0;
   partclassi->xyz=NULL;
   partclassi->maxpoints=0;
   partclassi->labels=NULL;

   NewMemory((void **)&partclassi->labels,sizeof(flowlabels));
   createnulllabel(partclassi->labels);

   npartclassinfo=0;


 }

  ibartemp=2;
  jbartemp=2;
  kbartemp=2;

  /* --------- set up multi-block data structures ------------- */

  /* 
     The keywords TRNX, TRNY, TRNZ, GRID, PDIM, OBST and VENT are not required 
     BUT if any one is present then the number of each must be equal 
  */

  if(nmeshes==0&&ntrnx==0&&ntrny==0&&ntrnz==0&&npdim==0&&nobst==0&&nvent==0&&noffset==0){
    nmeshes=1;
    ntrnx=1;
    ntrny=1;
    ntrnz=1;
    npdim=1;
    nobst=1;
    noffset=1;
  }
  else{
  if(nmeshes>1){
      if(nmeshes!=ntrnx||nmeshes!=ntrny||nmeshes!=ntrnz||
         nmeshes!=npdim||nmeshes!=nobst||nmeshes!=nvent||
         nmeshes!=noffset){
        printf("*** fatal error:\n");
        if(nmeshes!=ntrnx)printf("  found %i TRNX keywords, was expecting %i\n",ntrnx,nmeshes);
        if(nmeshes!=ntrny)printf("  found %i TRNY keywords, was expecting %i\n",ntrny,nmeshes);
        if(nmeshes!=ntrnz)printf("  found %i TRNZ keywords, was expecting %i\n",ntrnz,nmeshes);
        if(nmeshes!=npdim)printf("  found %i PDIM keywords, was expecting %i\n",npdim,nmeshes);
        if(nmeshes!=nobst)printf("  found %i OBST keywords, was expecting %i\n",nobst,nmeshes);
        if(nmeshes!=nvent)printf("  found %i VENT keywords, was expecting %i\n",nvent,nmeshes);
        if(nmeshes!=noffset)printf("  found %i OFFSET keywords, was expecting %i\n",noffset,nmeshes);
        return 2;
      }
     }
  }
  FREEMEMORY(meshinfo);
  if(NewMemory((void **)&meshinfo,nmeshes*sizeof(mesh))==0)return 2;
  meshinfo->plot3dfilenum=-1;
  update_current_mesh(meshinfo);
  for(n=0;n<nmeshes;n++){
    mesh *meshi;

    meshi=meshinfo+n;
    meshi->ibar=0;
    meshi->jbar=0;
    meshi->kbar=0;
    meshi->nbptrs=0;
    meshi->nvents=0;
    meshi->plotn=1;
    meshi->itextureoffset=0;

    meshi->nsmoothblockages_list=0;
    meshi->smoothblockages_list=NULL;
    meshi->nsmoothblockages_list++;
  }
  if(setPDIM==0){
    mesh *meshi;

    if(roomdefined==0){
      xbar0 = 0.0;    xbar = 1.0;
      ybar0 = 0.0;    ybar = 1.0;
      zbar0 = 0.0;    zbar = 1.0; 
    }
    meshi=meshinfo;
    meshi->xyz_bar0[XXX]=xbar0;
    meshi->xyz_bar[XXX] =xbar;
    meshi->xcen=(xbar+xbar0)/2.0;
    meshi->xyz_bar0[YYY]=ybar0;
    meshi->xyz_bar[YYY] =ybar;
    meshi->ycen=(ybar+ybar0)/2.0;
    meshi->xyz_bar0[ZZZ]=zbar0;
    meshi->xyz_bar[ZZZ] =zbar;
    meshi->zcen=(zbar+zbar0)/2.0;
  }

  // define labels and memory for default colorbars

  FREEMEMORY(partinfo);
  if(npartinfo!=0){
    if(NewMemory((void **)&partinfo,npartinfo*sizeof(partdata))==0)return 2;
  }

  FREEMEMORY(targinfo);
  if(ntarginfo!=0){
    if(NewMemory((void **)&targinfo,ntarginfo*sizeof(targ))==0)return 2;
  }

  FREEMEMORY(vsliceinfo);
  FREEMEMORY(sliceinfo);
  FREEMEMORY(slicetypes);
  FREEMEMORY(vslicetypes);
  FREEMEMORY(fedinfo);
  if(nsliceinfo>0){
    if(NewMemory( (void **)&vsliceinfo, 3*nsliceinfo*sizeof(vslicedata) )==0||
       NewMemory( (void **)&sliceinfo,  nsliceinfo*sizeof(slicedata)    )==0||
       NewMemory( (void **)&fedinfo,  nsliceinfo*sizeof(feddata)    )==0||
       NewMemory( (void **)&slicetypes, nsliceinfo*sizeof(int)      )==0||
       NewMemory( (void **)&slice_loadstack, nsliceinfo*sizeof(int)      )==0||
       NewMemory( (void **)&vslice_loadstack, nsliceinfo*sizeof(int)      )==0||
       NewMemory( (void **)&subslice_menuindex, nsliceinfo*sizeof(int)      )==0||
       NewMemory( (void **)&subvslice_menuindex, nsliceinfo*sizeof(int)      )==0||
       NewMemory( (void **)&mslice_loadstack, nsliceinfo*sizeof(int)      )==0||
       NewMemory( (void **)&mvslice_loadstack, nsliceinfo*sizeof(int)      )==0||
       NewMemory( (void **)&vslicetypes,3*nsliceinfo*sizeof(int)    )==0){
       return 2;
    }
    nslice_loadstack=nsliceinfo;
    islice_loadstack=0;
    nvslice_loadstack=nsliceinfo;
    ivslice_loadstack=0;
    nmslice_loadstack=nsliceinfo;
    imslice_loadstack=0;
    nmvslice_loadstack=nsliceinfo;
    imvslice_loadstack=0;
       
  }
  if(nsmoke3dinfo>0){
    if(NewMemory( (void **)&smoke3dinfo, nsmoke3dinfo*sizeof(smoke3ddata))==0)return 2;
  }

  FREEMEMORY(patchinfo);
  FREEMEMORY(patchtypes);
  if(npatchinfo!=0){
    if(NewMemory((void **)&patchinfo,npatchinfo*sizeof(patchdata))==0)return 2;
    for(i=0;i<npatchinfo;i++){
      patchdata *patchi;

      patchi = patchinfo + i;
      patchi->reg_file=NULL;
      patchi->comp_file=NULL;
      patchi->file=NULL;
      patchi->size_file=NULL;
    }
    if(NewMemory((void **)&patchtypes,npatchinfo*sizeof(int))==0)return 2;
  }
  FREEMEMORY(isoinfo);
  FREEMEMORY(isotypes);
  if(nisoinfo>0){
    if(NewMemory((void **)&isoinfo,nisoinfo*sizeof(isodata))==0)return 2;
    if(NewMemory((void **)&isotypes,nisoinfo*sizeof(int))==0)return 2;
  }
  FREEMEMORY(roominfo);
  if(nrooms>0){
    if(NewMemory((void **)&roominfo,(nrooms+1)*sizeof(roomdata))==0)return 2;
  }
  FREEMEMORY(fireinfo);
  if(nfires>0){
    if(NewMemory((void **)&fireinfo,nfires*sizeof(firedata))==0)return 2;
  }
  FREEMEMORY(zoneinfo);
  if(nzoneinfo>0){
    if(NewMemory((void **)&zoneinfo,nzoneinfo*sizeof(zonedata))==0)return 2;
  }
  FREEMEMORY(zventinfo);
  if(nzvents>0){
    if(NewMemory((void **)&zventinfo,nzvents*sizeof(zvent))==0)return 2;
  }
  nzvents=0;
  nzhvents=0;
  nzvvents=0;

  FREEMEMORY(textureinfo);
  FREEMEMORY(surfinfo);
  if(NewMemory((void **)&surfinfo,(nsurfinfo+10)*sizeof(surfdata))==0)return 2;

  if(cadgeominfo!=NULL)freecadinfo();
  if(ncadgeom>0){
    if(NewMemory((void **)&cadgeominfo,ncadgeom*sizeof(cadgeom))==0)return 2;
  }

  if(noutlineinfo>0){
    if(NewMemory((void **)&outlineinfo,noutlineinfo*sizeof(outline))==0)return 2;
    for(i=0;i<noutlineinfo;i++){
      outline *outlinei;

      outlinei = outlineinfo + i;
      outlinei->x1=NULL;
      outlinei->x2=NULL;
      outlinei->y1=NULL;
      outlinei->y2=NULL;
      outlinei->z1=NULL;
      outlinei->z2=NULL;
    }
  }
  if(nticks>0){
    if(NewMemory((void **)&tickinfo,nticks*sizeof(tickdata))==0)return 2;
    nticks=0;
    ntickssmv=0;
  }

  if(nlabels>0){
    if(NewMemory((void **)&labelinfo,nlabels*sizeof(labeldata))==0)return 2;
    nlabels=0;
    nlabelssmv=0;
  }

  if(npropinfo>0){
    npropinfo=0;
    init_default_prop();
    npropinfo=1;
  }

/* 
   ************************************************************************
   ************************ start of pass 2 ********************************* 
   ************************************************************************
 */

  startpass=1;
  ioffset=0;
  iobst=0;
  ncadgeom=0;
  nsurfinfo=0;
  noutlineinfo=0;
  if(noffset==0)ioffset=1;
  rewind(stream1);
  if(stream2!=NULL)rewind(stream2);
  stream=stream1;
  printf("%s",_("   pass 1"));
  printf("%s",_(" completed"));
  printf("\n");
  printf("%s",_("   pass 2 started"));
  printf("\n");
  for(;;){
    if(feof(stream)!=0){
      BREAK;
    }
    if(noGRIDpresent==1&&startpass==1){
      strcpy(buffer,"GRID");
      startpass=0;
    }
    else{
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      trim(buffer);
      if(strncmp(buffer," ",1)==0||buffer[0]==0)continue;
    }


    /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++ CSVF ++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"CSVF") == 1){
      csvdata *csvi;
      char *type_ptr, *file_ptr;
      int nfiles=1;

      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      trim(buffer);
      type_ptr=trim_front(buffer);

      if(fgets(buffer2,255,stream)==NULL){
        BREAK;
      }
      trim(buffer2);
      file_ptr=trim_front(buffer2);
      nfiles=1;
      if(strcmp(type_ptr,"hrr")==0){
        if(file_exists(file_ptr)==0)nfiles=0;
      }
      else if(strcmp(type_ptr,"devc")==0){
        if(file_exists(file_ptr)==0)nfiles=0;
      }
      else if(strcmp(type_ptr,"ext")==0){
        if(strchr(file_ptr,'*')==NULL){
          if(file_exists(file_ptr)==0)nfiles=0;
        }
        else{
          nfiles = get_nfilelist(".",file_ptr);
        }
      }
      else{
        nfiles=0;
      }
      if(nfiles==0)continue;

      csvi = csvinfo + ncsvinfo;

      csvi->loaded=0;
      csvi->display=0;

      if(strcmp(type_ptr,"hrr")==0){
        if(file_exists(file_ptr)==1){
          csvi->type=CSVTYPE_HRR;
          NewMemory((void **)&csvi->file,strlen(file_ptr)+1);
          strcpy(csvi->file,file_ptr);
        }
        else{
          nfiles=0;
        }
      }
      else if(strcmp(type_ptr,"devc")==0){
        if(file_exists(file_ptr)==1){
          csvi->type=CSVTYPE_DEVC;
          NewMemory((void **)&csvi->file,strlen(file_ptr)+1);
          strcpy(csvi->file,file_ptr);
        }
        else{
          nfiles=0;
        }
      }
      else if(strcmp(type_ptr,"ext")==0){
        if(strchr(file_ptr,'*')==NULL){
          if(file_exists(file_ptr)==1){
            csvi->type=CSVTYPE_EXT;
            NewMemory((void **)&csvi->file,strlen(file_ptr)+1);
            strcpy(csvi->file,file_ptr);
          }
          else{
            nfiles=0;
          }
        }
        else{
          filelistdata *filelist;
          int nfilelist;

          nfilelist = get_nfilelist(".",file_ptr);
          nfiles=get_filelist(".",file_ptr,nfilelist,&filelist);
          for(i=0;i<nfiles;i++){
            csvi = csvinfo + ncsvinfo + i;
            csvi->loaded=0;
            csvi->display=0;
            csvi->type=CSVTYPE_EXT;
            NewMemory((void **)&csvi->file,strlen(filelist[i].file)+1);
            strcpy(csvi->file,filelist[i].file);
          }
          free_filelist(filelist,&nfilelist);
        }
      }
      else{
        nfiles=0;
      }

      ncsvinfo+=nfiles;
      continue;
    }

    /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++ GEOM ++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"GEOM") == 1){
      geomdata *geomi;
      char *buff2;

      fgets(buffer,255,stream);
      geomi = geominfo + ngeominfo;
      trim(buffer);
      buff2 = trim_front(buffer);
      NewMemory((void **)&geomi->file,strlen(buff2)+1);
      strcpy(geomi->file,buff2);

      init_geom(geomi);

      ngeominfo++;
      continue;
    }

    /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++ OBST ++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"OBST") == 1&&autoterrain==1){
      int nobsts=0;
      mesh *meshi;
      unsigned char *is_block_terrain;
      
      iobst++;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&nobsts);


      meshi=meshinfo+iobst-1;

      if(nobsts<=0)continue;

      NewMemory((void **)&meshi->is_block_terrain,nobsts*sizeof(unsigned char));
      is_block_terrain=meshi->is_block_terrain;

      for(nn=0;nn<nobsts;nn++){
        fgets(buffer,255,stream);
      }
      for(nn=0;nn<nobsts;nn++){
        int ijk2[6],colorindex_local=0,blocktype_local=-1;

        fgets(buffer,255,stream);
        sscanf(buffer,"%i %i %i %i %i %i %i %i",ijk2,ijk2+1,ijk2+2,ijk2+3,ijk2+4,ijk2+5,&colorindex_local,&blocktype_local);
        if(blocktype_local>=0&&(blocktype_local&8)==8){
          is_block_terrain[nn]=1;
        }
        else{
          is_block_terrain[nn]=0;
        }
      }
      continue;
    }

    /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++ PROP ++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

    if(match(buffer,"PROP") == 1){
      propdata *propi;
      char *fbuffer;
      char proplabel[255];
      int lenbuf;
      int ntextures_local;
      int nsmokeview_ids;
      char *smokeview_id;

      propi = propinfo + npropinfo;

      if(fgets(proplabel,255,stream)==NULL){
        BREAK;  // prop label
      }
      trim(proplabel);
      fbuffer=trim_front(proplabel);

      if(fgets(buffer,255,stream)==NULL){
        BREAK;  // number of smokeview_id's
      }
      sscanf(buffer,"%i",&nsmokeview_ids);

      init_prop(propi,nsmokeview_ids,fbuffer);
      for(i=0;i<nsmokeview_ids;i++){
        if(fgets(buffer,255,stream)==NULL){
          BREAK; // smokeview_id
        }
        trim(buffer);
        fbuffer=trim_front(buffer);
        lenbuf=strlen(fbuffer);
        NewMemory((void **)&smokeview_id,lenbuf+1);
        strcpy(smokeview_id,fbuffer);
        propi->smokeview_ids[i]=smokeview_id;
        propi->smv_objects[i]=get_SVOBJECT_type(propi->smokeview_ids[i],missing_device);
      }
      propi->smv_object=propi->smv_objects[0];
      propi->smokeview_id=propi->smokeview_ids[0];

      if(fgets(buffer,255,stream)==NULL){
        BREAK; // keyword_values
      }
      sscanf(buffer,"%i",&propi->nvars_indep);
      propi->vars_indep=NULL;
      propi->svals=NULL;
      propi->texturefiles=NULL;
      ntextures_local=0;
      if(propi->nvars_indep>0){
        NewMemory((void **)&propi->vars_indep,propi->nvars_indep*sizeof(char *));
        NewMemory((void **)&propi->svals,propi->nvars_indep*sizeof(char *));
        NewMemory((void **)&propi->fvals,propi->nvars_indep*sizeof(float));
        NewMemory((void **)&propi->vars_indep_index,propi->nvars_indep*sizeof(int));
        NewMemory((void **)&propi->texturefiles,propi->nvars_indep*sizeof(char *));

        for(i=0;i<propi->nvars_indep;i++){
          char *equal;

          propi->svals[i]=NULL;
          propi->vars_indep[i]=NULL;
          propi->fvals[i]=0.0;
          fgets(buffer,255,stream);
          equal=strchr(buffer,'=');
          if(equal!=NULL){
            char *buf1, *buf2, *keyword, *val;
            int lenkey, lenval;
            char *texturefile;

            buf1=buffer;
            buf2=equal+1;
            *equal=0;

            trim(buf1);
            keyword=trim_front(buf1);
            lenkey=strlen(keyword);

            trim(buf2);
            val=trim_front(buf2);
            lenval=strlen(val);

            if(lenkey==0||lenval==0)continue;

            if(val[0]=='"'){
              val[0]=' ';
              if(val[lenval-1]=='"')val[lenval-1]=' ';
              trim(val);
              val=trim_front(val);
              NewMemory((void **)&propi->svals[i],lenval+1);
              strcpy(propi->svals[i],val);
              texturefile=strstr(val,"t%");
              if(texturefile!=NULL){
                texturefile+=2;
                texturefile=trim_front(texturefile);
                propi->texturefiles[ntextures_local]=propi->svals[i];
                strcpy(propi->svals[i],texturefile);

                ntextures_local++;
              }
            }

            NewMemory((void **)&propi->vars_indep[i],lenkey+1);
            strcpy(propi->vars_indep[i],keyword);

            sscanf(val,"%f",propi->fvals+i);
          }
        }
        get_indep_var_indices(propi->smv_object,propi->vars_indep,propi->nvars_indep,propi->vars_indep_index);
        get_evac_indices(propi->smv_object,propi->fvars_evac_index,&propi->nvars_evac);

      }
      propi->ntextures=ntextures_local;
      npropinfo++;
      continue;
    }

    /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++ TERRAIN +++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

    if(match(buffer,"TERRAIN") == 1){
//      terraindata *terri;
      float xmin, xmax, ymin, ymax;
      int nx, ny;

      manual_terrain=1;

  //    terri = terraininfo + nterraininfo;

      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %i %f %f %i",&xmin, &xmax, &nx, &ymin, &ymax, &ny);
      // must implement new form for defining terrain surfaces
      //initterrain(stream, NULL, terri, xmin, xmax, nx, ymin, ymax, ny);

      nterraininfo++;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++ CLASS_OF_PARTICLES +++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"CLASS_OF_PARTICLES") == 1||
       match(buffer,"CLASS_OF_HUMANS") == 1){
      float rgb_class[4];
      part5class *partclassi;
      char *device_ptr;
      char *prop_id;
      char prop_buffer[255];

      partclassi = partclassinfo + npartclassinfo;
      partclassi->kind=PARTICLES;
      if(match(buffer,"CLASS_OF_HUMANS") == 1)partclassi->kind=HUMANS;
      fgets(buffer,255,stream);

      get_labels(buffer,partclassi->kind,&device_ptr,&prop_id,prop_buffer);
      if(prop_id!=NULL){
        device_ptr=NULL;
      }
      partclassi->prop=NULL;

      partclassi->sphere=NULL;
      partclassi->smv_device=NULL;
      partclassi->device_name=NULL;
      if(device_ptr!=NULL){
        partclassi->sphere=get_SVOBJECT_type("SPHERE",missing_device);

        partclassi->smv_device=get_SVOBJECT_type(device_ptr,missing_device);
        if(partclassi->smv_device!=NULL){
          len = strlen(device_ptr);
          NewMemory((void **)&partclassi->device_name,len+1);
          STRCPY(partclassi->device_name,device_ptr);
        }
        else{
          char tube[10];

          strcpy(tube,"TUBE");
          len = strlen(tube);
          NewMemory((void **)&partclassi->device_name,len+1);
          STRCPY(partclassi->device_name,tube);
          partclassi->smv_device=get_SVOBJECT_type(tube,missing_device);
        }
      }

      trim(buffer);
      len=strlen(buffer);
      partclassi->name=NULL;
      if(len>0){
        NewMemory((void **)&partclassi->name,len+1);
        STRCPY(partclassi->name,trim_front(buffer));
      }

      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",rgb_class,rgb_class+1,rgb_class+2);
      rgb_class[3]=1.0;
      partclassi->rgb=getcolorptr(rgb_class);

      partclassi->ntypes=0;
      partclassi->xyz=NULL;
      partclassi->maxpoints=0;
      partclassi->labels=NULL;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&partclassi->ntypes);
      partclassi->ntypes+=2;
      partclassi->nvars_dep=partclassi->ntypes-2+3; // subtract off two "dummies" at beginning and add 3 at end for r,g,b
      if(partclassi->ntypes>0){
        flowlabels *labelj;
        char shortdefaultlabel[]="Uniform";
        char longdefaultlabel[]="Uniform color";

        NewMemory((void **)&partclassi->labels,partclassi->ntypes*sizeof(flowlabels));
       
        labelj = partclassi->labels; // placeholder for hidden

        labelj->longlabel=NULL;
        labelj->shortlabel=NULL;
        labelj->unit=NULL;

        labelj = partclassi->labels+1;  // placeholder for default

        labelj->longlabel=NULL;
        NewMemory((void **)&labelj->longlabel,strlen(longdefaultlabel)+1);
        strcpy(labelj->longlabel,longdefaultlabel);
        labelj->shortlabel=NULL;
        NewMemory((void **)&labelj->shortlabel,strlen(shortdefaultlabel)+1);
        strcpy(labelj->shortlabel,shortdefaultlabel);
        labelj->unit=NULL;

        partclassi->col_azimuth=-1;
        partclassi->col_diameter=-1;
        partclassi->col_elevation=-1;
        partclassi->col_length=-1;
        partclassi->col_u_vel=-1;
        partclassi->col_v_vel=-1;
        partclassi->col_w_vel=-1;
        partclassi->vis_type=PART_POINTS;
        for(j=2;j<partclassi->ntypes;j++){
          labelj = partclassi->labels+j;
          labelj->longlabel=NULL;
          labelj->shortlabel=NULL;
          labelj->unit=NULL;
          readlabels(labelj,stream);
          partclassi->vars_dep[j-2]=labelj->shortlabel;
          if(strcmp(labelj->shortlabel,"DIAMETER")==0){
            partclassi->col_diameter=j-2;
          }
          if(strcmp(labelj->shortlabel,"LENGTH")==0){
            partclassi->col_length=j-2;
          }
          if(strcmp(labelj->shortlabel,"AZIMUTH")==0){
            partclassi->col_azimuth=j-2;
          }
          if(strcmp(labelj->shortlabel,"ELEVATION")==0){
            partclassi->col_elevation=j-2;
          }
          if(strcmp(labelj->shortlabel,"U-VEL")==0){
            partclassi->col_u_vel=j-2;
          }
          if(strcmp(labelj->shortlabel,"V-VEL")==0){
            partclassi->col_v_vel=j-2;
          }
          if(strcmp(labelj->shortlabel,"W-VEL")==0){
            partclassi->col_w_vel=j-2;
          }
        }
      }
      partclassi->diameter=1.0;
      partclassi->length=1.0;
      partclassi->azimuth=0.0;
      partclassi->elevation=0.0;
      partclassi->dx=0.0;
      partclassi->dy=0.0;
      partclassi->dz=0.0;
      if(device_ptr!=NULL){
        float diameter, length, azimuth, elevation;

        fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f %f",&diameter,&length,&azimuth,&elevation);
        partclassi->diameter=diameter;
        partclassi->length=length;
        partclassi->azimuth=azimuth;
        partclassi->elevation=elevation;
      }
      npartclassinfo++;
      continue;
    }

  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ LABEL ++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

    if(match(buffer,"LABEL") == 1){
      nlabels++;
      nlabelssmv++;

      /*
      LABEL
      x y z r g b tstart tstop  
      label

      */
      {
        float *xyz, *rgbtemp, *tstart_stop;
        labeldata *labeli;

        labeli = labelinfo + nlabels-1;

        xyz = labeli->xyz;
        rgbtemp = labeli->rgb;
        tstart_stop = labeli->tstart_stop;

        fgets(buffer,255,stream);
        rgbtemp[0]=-1.0;
        rgbtemp[1]=-1.0;
        rgbtemp[2]=-1.0;
        rgbtemp[3]=1.0;
        tstart_stop[0]=-1.0;
        tstart_stop[1]=-1.0;
        sscanf(buffer,"%f %f %f %f %f %f %f %f",
          xyz,xyz+1,xyz+2,
          rgbtemp,rgbtemp+1,rgbtemp+2,
          tstart_stop,tstart_stop+1);
        if(rgbtemp[0]<0.0||rgbtemp[1]<0.0||rgbtemp[2]<0.0||rgbtemp[0]>1.0||rgbtemp[1]>1.0||rgbtemp[2]>1.0){
          labeli->useforegroundcolor=1;
        }
        else{
          labeli->useforegroundcolor=0;
        }
        fgets(buffer,255,stream);
        strcpy(labeli->label,buffer);
      }
      continue;
    }

  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TICKS ++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

    if(match(buffer,"TICKS") == 1){
      nticks++;
      ntickssmv++;
      {
        tickdata *ticki;
        float *begt, *endt;
        int *nbarst;
        float term;
        float length=0.0;
        float *dxyz;
        float sum;

        ticki = tickinfo + nticks - 1;
        begt = ticki->begin;
        endt = ticki->end;
        nbarst=&ticki->nbars;
        dxyz = ticki->dxyz;

        if(fgets(buffer,255,stream)==NULL){
          BREAK;
        }
        *nbarst=0;
        sscanf(buffer,"%f %f %f %f %f %f %i",begt,begt+1,begt+2,endt,endt+1,endt+2,nbarst);
        if(*nbarst<1)*nbarst=1;
        if(fgets(buffer,255,stream)==NULL){
          BREAK;
        }
        {
          float *rgbtemp;

          rgbtemp=ticki->rgb;
          rgbtemp[0]=-1.0;
          rgbtemp[1]=-1.0;
          rgbtemp[2]=-1.0;
          ticki->width=-1.0;
          sscanf(buffer,"%f %i %f %f %f %f",&ticki->dlength,&ticki->dir,rgbtemp,rgbtemp+1,rgbtemp+2,&ticki->width);
          if(rgbtemp[0]<0.0||rgbtemp[0]>1.0||
             rgbtemp[1]<0.0||rgbtemp[1]>1.0||
             rgbtemp[2]<0.0||rgbtemp[2]>1.0){
            ticki->useforegroundcolor=1;
          }
          else{
            ticki->useforegroundcolor=0;
          }
          if(ticki->width<0.0)ticki->width=1.0;
        }
        for(i=0;i<3;i++){
          term = endt[i]-begt[i];
          length += term*term;
        }
        if(length<=0.0){
          endt[0]=begt[0]+1.0;
          length = 1.0;
        }
        ticki->length=sqrt(length);
        dxyz[0] =  0.0;
        dxyz[1] =  0.0;
        dxyz[2] =  0.0;
        switch (ticki->dir){
        case 1:
        case -1:
          dxyz[0]=1.0;
          break;
        case 2:
        case -2:
          dxyz[1]=1.0;
          break;
        case 3:
        case -3:
          dxyz[2]=1.0;
          break;
        default:
          ASSERT(FFALSE);
          break;
        }
        if(ticki->dir<0){
          for(i=0;i<3;i++){
            dxyz[i]=-dxyz[i];
          }
        }
        sum = 0.0;
        sum = dxyz[0]*dxyz[0] + dxyz[1]*dxyz[1] + dxyz[2]*dxyz[2];
        if(sum>0.0){
          sum=sqrt(sum);
          dxyz[0] *= (ticki->dlength/sum);
          dxyz[1] *= (ticki->dlength/sum);
          dxyz[2] *= (ticki->dlength/sum);
        }
      }
      continue;
    }


  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ OUTLINE ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

    if(match(buffer,"OUTLINE") == 1){
      outline *outlinei;

      noutlineinfo++;
      outlinei = outlineinfo + noutlineinfo - 1;
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      sscanf(buffer,"%i",&outlinei->nlines);
      if(outlinei->nlines>0){
        x1=NULL;
        x2=NULL;
        yy1=NULL;
        yy2=NULL;
        z1=NULL;
        z2=NULL;
        NewMemory((void **)&outlinei->x1,outlinei->nlines*sizeof(float));
        NewMemory((void **)&outlinei->y1,outlinei->nlines*sizeof(float));
        NewMemory((void **)&outlinei->z1,outlinei->nlines*sizeof(float));
        NewMemory((void **)&outlinei->x2,outlinei->nlines*sizeof(float));
        NewMemory((void **)&outlinei->y2,outlinei->nlines*sizeof(float));
        NewMemory((void **)&outlinei->z2,outlinei->nlines*sizeof(float));
        for(i=0;i<outlinei->nlines;i++){
          fgets(buffer,255,stream);
          sscanf(buffer,"%f %f %f %f %f %f",
            outlinei->x1+i,outlinei->y1+i, outlinei->z1+i,
            outlinei->x2+i,outlinei->y2+i, outlinei->z2+i
            );
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ CADGEOM ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"CADGEOM") == 1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      bufferptr=trim_string(buffer);
      len=strlen(bufferptr);
      cadgeominfo[ncadgeom].order=NULL;
      cadgeominfo[ncadgeom].quad=NULL;
      cadgeominfo[ncadgeom].file=NULL;
      if(STAT(bufferptr,&statbuffer)==0){
        if(NewMemory((void **)&cadgeominfo[ncadgeom].file,(unsigned int)(len+1))==0)return 2;
        STRCPY(cadgeominfo[ncadgeom].file,bufferptr);
        printf("%s %s",_("     reading cad file: "),bufferptr);
        printf("%s\n",bufferptr);
        readcadgeom(cadgeominfo+ncadgeom);
        printf("     CAD file reading completed\n");
        ncadgeom++;
      }
      else{
        printf(_("   CAD geometry file: %s could not be opened"),bufferptr);
        printf("\n");
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ OFFSET ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"OFFSET") == 1){
      ioffset++;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SURFDEF ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SURFDEF") == 1){
      fgets(buffer,255,stream);
      bufferptr=trim_string(buffer);
      strcpy(surfacedefaultlabel,trim_front(bufferptr));
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SMOKE3D ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(
      match(buffer,"SMOKE3D") == 1||
      match(buffer,"VSMOKE3D") == 1||
      match(buffer,"SMOKF3D") == 1||
      match(buffer,"VSMOKF3D") == 1
      ){

      size_t lenbuffer;
      float temp_val=-1.0;
      char *buffer_temp;
      int filetype=0;

      if(match(buffer,"SMOKF3D") == 1||match(buffer,"VSMOKF3D") == 1){
        filetype=1;
      }

      if(match(buffer,"VSMOKE3D") == 1||match(buffer,"VSMOKF3D") == 1){
        buffer_temp=buffer+8;
        sscanf(buffer_temp,"%i %f",&idummy,&temp_val);
        if(temp_val>0.0)hrrpuv_max_smv=temp_val;
      }
      nn_smoke3d++;
      trim(buffer);
      len=strlen(buffer);
      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(len>8){
        buffer3=buffer+8;
        sscanf(buffer3,"%i",&blocknumber);
        blocknumber--;
      }
      if(fgets(buffer,255,stream)==NULL){
        nsmoke3dinfo--;
        BREAK;
      }
      bufferptr=trim_string(buffer);
      len=strlen(buffer);
      lenbuffer=len;
      {
        smoke3ddata *smoke3di;
        
        smoke3di = smoke3dinfo + ismoke3d;
    
        if(nsmoke3dinfo>50&&(ismoke3d%10==0||ismoke3d==nsmoke3dinfo-1)){
          if(ismoke3dcount==11){
            printf("     examining %i'th 3D smoke file\n",ismoke3dcount);
          }
          else{
            printf("     examining %i'st 3D smoke file\n",ismoke3dcount);
          }
        }
        ismoke3dcount++;

        if(NewMemory((void **)&smoke3di->reg_file,(unsigned int)(len+1))==0)return 2;
        STRCPY(smoke3di->reg_file,bufferptr);

        smoke3di->filetype=filetype;
        smoke3di->is_zlib=0;
        smoke3di->seq_id=nn_smoke3d;
        smoke3di->autoload=0;
        smoke3di->compression_type=UNKNOWN;
        smoke3di->hrrpuv_color=NULL;
        smoke3di->water_color=NULL;
        smoke3di->soot_color=NULL;
        smoke3di->file=NULL;
        smoke3di->smokeframe_in=NULL;
        smoke3di->smokeframe_comp_list=NULL;
        smoke3di->smokeframe_out=NULL;
        smoke3di->timeslist=NULL;
        smoke3di->smoke_comp_all=NULL;
        smoke3di->smokeview_tmp=NULL;
        smoke3di->times=NULL;
        smoke3di->use_smokeframe=NULL;
        smoke3di->nchars_compressed_smoke=NULL;
        smoke3di->nchars_compressed_smoke_full=NULL;
        smoke3di->frame_all_zeros=NULL;

        smoke3di->display=0;
        smoke3di->loaded=0;
        smoke3di->d_display=0;
        smoke3di->blocknumber=blocknumber;
        smoke3di->lastiframe=-999;
        smoke3di->soot_index=-1;
        smoke3di->water_index=-1;
        smoke3di->hrrpuv_index=-1;

        STRCPY(buffer2,bufferptr);
        STRCAT(buffer2,".svz");

        len=lenbuffer+4;
        if(NewMemory((void **)&smoke3di->comp_file,(unsigned int)(len+1))==0)return 2;
        STRCPY(smoke3di->comp_file,buffer2);

        if(file_exists(smoke3di->comp_file)==1){
          smoke3di->file=smoke3di->comp_file;
          smoke3di->is_zlib=1;
        }
        else{
          smoke3di->file=smoke3di->reg_file;
        }
        if(file_exists(smoke3di->file)==1){
          if(readlabels(&smoke3di->label,stream)==2)return 2;
          if(strcmp(smoke3di->label.longlabel,"HRRPUV")==0){
            show_hrrcutoff_active=1;
          }
          ismoke3d++;
        }
        else{
          if(readlabels(&smoke3di->label,stream)==2)return 2;
          nsmoke3dinfo--;
        }
        if(smoke3di->have_light==1)have_lighting=1;
        if(strncmp(smoke3di->label.shortlabel,"soot",4)==0){
          smoke3di->type=1;
        }
        else if(strncmp(smoke3di->label.shortlabel,"hrrpuv",6)==0){
          smoke3di->type=2;
        }
        else if(strncmp(smoke3di->label.shortlabel,"water",5)==0){
          smoke3di->type=3;
        }
        else{
          smoke3di->type=1;
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SURFACE ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SURFACE") ==1){
      surfdata *surfi;

      surfi = surfinfo + nsurfinfo;
      initsurface(surfi);
      fgets(buffer,255,stream);
      trim(buffer);
      len=strlen(buffer);
      NewMemory((void **)&surfi->surfacelabel,(len+1)*sizeof(char));
      strcpy(surfi->surfacelabel,trim_front(buffer));
      if(strstr(surfi->surfacelabel,"MIRROR")!=NULL||
         strstr(surfi->surfacelabel,"INTERPOLATED")!=NULL||
         strstr(surfi->surfacelabel,"OPEN")!=NULL){
        surfi->obst_surface=0;
      }
      if(strstr(surfi->surfacelabel,"INERT")!=NULL){
        surfinfo[0].obst_surface=1;
      }

      temp_ignition=TEMP_IGNITION_MAX;
      emis = 1.0;
      t_width=1.0;
      t_height=1.0;
      s_type=0;
      s_color[0]=surfi->color[0];
      s_color[1]=surfi->color[1];
      s_color[2]=surfi->color[2];
      //s_color[3]=1.0;
      s_color[3]=surfi->color[3];
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f",&temp_ignition,&emis);
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %f %f %f %f %f",
        &s_type,&t_width, &t_height,s_color,s_color+1,s_color+2,s_color+3);

      surfi->type=s_type;
      if(s_color[0]<=0.0&&s_color[1]<=0.0&&s_color[2]<=0.0){
        if(s_color[0]!=0.0||s_color[1]!=0.0||s_color[2]!=0.0){
          surfi->invisible=1;
          surfi->type=BLOCK_hidden;
          s_color[0]=-s_color[0];
          s_color[1]=-s_color[1];
          s_color[2]=-s_color[2];
        }
      }
      surfi->color = getcolorptr(s_color);
      if(s_color[3]<0.99){
        surfi->transparent=1;
      }
      surfi->transparent_level=1.0;
      surfi->temp_ignition=temp_ignition;
      surfi->emis=emis;
      surfi->t_height=t_height;
      surfi->t_width=t_width;
      surfi->texturefile=NULL;
      surfi->textureinfo=NULL;

      fgets(buffer,255,stream);
      trim(buffer);
      buffer3 = trim_front(buffer);
      {
        int found_texture;
        char texturebuffer[1024];

        found_texture=0;
        if(texturedir!=NULL&&STAT(buffer3,&statbuffer)!=0){
          STRCPY(texturebuffer,texturedir);
          STRCAT(texturebuffer,dirseparator);
          STRCAT(texturebuffer,buffer3);
          if(STAT(texturebuffer,&statbuffer)==0){
            if(NewMemory((void **)&surfi->texturefile,strlen(texturebuffer)+1)==0)return 2;
            STRCPY(surfi->texturefile,texturebuffer);
            found_texture=1;
          }
        }
        if(STAT(buffer3,&statbuffer)==0){
          len=strlen(buffer3);
          if(NewMemory((void **)&surfi->texturefile,(unsigned int)(len+1))==0)return 2;
          STRCPY(surfi->texturefile,buffer3);
          found_texture=1;
        }
        if(texturebuffer!=NULL&&buffer3!=NULL&&found_texture==0&&strncmp(buffer3,"null",4)!=0){
          char message[1024];

          strcpy(message,"The texture file: ");
          strcat(message,buffer3);
          strcat(message," was not found.");
          warning_message(message);
        }
      }
      nsurfinfo++;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ GRID ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"GRID") == 1){
      mesh *meshi;
      int mesh_type=0;
      float *xp, *yp, *zp;
      float *xp2, *yp2, *zp2;
      float *xplt_cen, *yplt_cen,*zplt_cen;

//      int lenbuffer;

//      trim(buffer);
//      lenbuffer=strlen(buffer);
//      if(lenbuffer>4){
//        if(buffer[5]!=' ')continue;
//      }

      igrid++;
      if(meshinfo!=NULL){
        meshi=meshinfo+igrid-1;
        initmesh(meshi);
        {
          size_t len_meshlabel;
          char *meshlabel;

          len_meshlabel=0;
          if(strlen(buffer)>5){
            meshlabel=trim_front(buffer+5);
            trim(meshlabel);
            len_meshlabel=strlen(meshlabel);
          }
          if(len_meshlabel>0){
            NewMemory((void **)&meshi->label,(len_meshlabel+1));
            strcpy(meshi->label,meshlabel);
          }
          else{
            sprintf(buffer,"%i",igrid);
            NewMemory((void **)&meshi->label,strlen(buffer)+1);
            strcpy(meshi->label,buffer);
          }
        }

      }
      setGRID=1;
      if(noGRIDpresent==1){
        ibartemp=2;
        jbartemp=2;
        kbartemp=2;
      }
      else{
        fgets(buffer,255,stream);
        sscanf(buffer,"%i %i %i %i",&ibartemp,&jbartemp,&kbartemp,&mesh_type);
      }
      if(ibartemp<1)ibartemp=1;
      if(jbartemp<1)jbartemp=1;
      if(kbartemp<1)kbartemp=1;
      xp=NULL; yp=NULL; zp=NULL;
      xp2=NULL; yp2=NULL; zp2=NULL;
      if(
         NewMemory((void **)&xp,sizeof(float)*(ibartemp+1))==0||
         NewMemory((void **)&yp,sizeof(float)*(jbartemp+1))==0||
         NewMemory((void **)&zp,sizeof(float)*(kbartemp+1))==0||
         NewMemory((void **)&xplt_cen,sizeof(float)*ibartemp)==0||
         NewMemory((void **)&yplt_cen,sizeof(float)*jbartemp)==0||
         NewMemory((void **)&zplt_cen,sizeof(float)*kbartemp)==0||
         NewMemory((void **)&xp2,sizeof(float)*(ibartemp+1))==0||
         NewMemory((void **)&yp2,sizeof(float)*(jbartemp+1))==0||
         NewMemory((void **)&zp2,sizeof(float)*(kbartemp+1))==0
         )return 2;
      if(meshinfo!=NULL){
        meshi->mesh_type=mesh_type;
        meshi->xplt=xp;
        meshi->yplt=yp;
        meshi->zplt=zp;
        meshi->xplt_cen=xplt_cen;
        meshi->yplt_cen=yplt_cen;
        meshi->zplt_cen=zplt_cen;
        meshi->xplt_orig=xp2;
        meshi->yplt_orig=yp2;
        meshi->zplt_orig=zp2;
        meshi->ibar=ibartemp;
        meshi->jbar=jbartemp;
        meshi->kbar=kbartemp;
        meshi->plotx=ibartemp/2;
        meshi->ploty=jbartemp/2;
        meshi->plotz=kbartemp/2;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ ZONE ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"ZONE") == 1){
      char *filename;
      zonedata *zonei;
      char buffer_csv[1000],*buffer_csvptr;
      char *period=NULL;

      zonei = zoneinfo + izone_local;
      if(fgets(buffer,255,stream)==NULL){
        nzoneinfo--;
        BREAK;
      }
      bufferptr=trim_string(buffer);
      len=strlen(bufferptr);
      zonei->loaded=0;
      zonei->display=0;

      buffer_csvptr=buffer_csv;
      strcpy(buffer_csv,bufferptr);
      filename=get_zonefilename(buffer_csvptr);
      if(filename!=NULL)period=strrchr(filename,'.');
      if(filename!=NULL&&period!=NULL&&strcmp(period,".csv")==0){
        zonei->csv=1;
        zonecsv=1;
      }
      else{
        zonei->csv=0;
        zonecsv=0;
        filename=get_zonefilename(bufferptr);
      }

      if(filename==NULL){
        for(n=0;n<4;n++){
          if(readlabels(&zonei->label[n],stream)==2){
            return 2;
          }
        }
        nzoneinfo--;
      }
      else{
        len=strlen(filename);
        NewMemory((void **)&zonei->file,(unsigned int)(len+1));
        STRCPY(zonei->file,filename);
        for(n=0;n<4;n++){
          if(readlabels(&zonei->label[n],stream)==2){
            return 2;
          }
        }
        izone_local++;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ AMBIENT ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"AMBIENT")==1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      sscanf(buffer,"%f %f %f",&pref,&pamb,&tamb);
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ ROOM ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"ROOM") == 1){
      roomdata *roomi;

      isZoneFireModel=1;
      visFrame=0;
      roomdefined=1;
      iroom++;
      roomi = roominfo + iroom - 1;
      roomi->valid=0;
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      sscanf(buffer,"%f %f %f",&roomi->dx,&roomi->dy,&roomi->dz);
      roomi->valid=1;
      if(fgets(buffer,255,stream)==NULL){
        roomi->x0=0.0;
        roomi->y0=0.0;
        roomi->z0=0.0;
      }
      else{
        sscanf(buffer,"%f %f %f",&roomi->x0,&roomi->y0,&roomi->z0);
      }
      roomi->x1=roomi->x0+roomi->dx;
      roomi->y1=roomi->y0+roomi->dy;
      roomi->z1=roomi->z0+roomi->dz;

      if(setPDIM==0){
        if(roomi->x0<xbar0)xbar0=roomi->x0;
        if(roomi->y0<ybar0)ybar0=roomi->y0;
        if(roomi->z0<zbar0)zbar0=roomi->z0;
        if(roomi->x1>xbar)xbar=roomi->x1;
        if(roomi->y1>ybar)ybar=roomi->y1;
        if(roomi->z1>zbar)zbar=roomi->z1;
      }
      continue;
    }
  }
/* 
   ************************************************************************
   ************************ end of pass 2 ********************************* 
   ************************************************************************
 */

  printf("%s",_("   pass 2 "));
  printf("%s",_("completed"));
  printf("\n");

  CheckMemory;
  parsedatabase(databasefilename);


  if(setGRID==0){
    mesh *meshi;
    float *xp, *yp, *zp;
    float *xp2, *yp2, *zp2;
    float *xplt_cen, *yplt_cen, *zplt_cen;

    xp=NULL; yp=NULL; zp=NULL;
    xp2=NULL; yp2=NULL; zp2=NULL;
    if(
       NewMemory((void **)&xp,sizeof(float)*(ibartemp+1))==0||
       NewMemory((void **)&yp,sizeof(float)*(jbartemp+1))==0||
       NewMemory((void **)&zp,sizeof(float)*(kbartemp+1))==0||
       NewMemory((void **)&xplt_cen,sizeof(float)*ibartemp)==0||
       NewMemory((void **)&yplt_cen,sizeof(float)*jbartemp)==0||
       NewMemory((void **)&zplt_cen,sizeof(float)*kbartemp)==0||
       NewMemory((void **)&xp2,sizeof(float)*(ibartemp+1))==0||
       NewMemory((void **)&yp2,sizeof(float)*(jbartemp+1))==0||
       NewMemory((void **)&zp2,sizeof(float)*(kbartemp+1))==0
       )return 2;
    for(nn=0;nn<=ibartemp;nn++){
      xp[nn]=xbar0+(float)nn*(xbar-xbar0)/(float)ibartemp;
    }
    for(nn=0;nn<=jbartemp;nn++){
      yp[nn]=ybar0+(float)nn*(ybar-ybar0)/(float)jbartemp;
    }
    for(nn=0;nn<=kbartemp;nn++){
      zp[nn]=zbar0+(float)nn*(zbar-zbar0)/(float)kbartemp;
    }
    meshi=meshinfo;
    initmesh(meshi);
    meshi->xplt=xp;
    meshi->yplt=yp;
    meshi->zplt=zp;
    meshi->xplt_cen=xplt_cen;
    meshi->yplt_cen=yplt_cen;
    meshi->zplt_cen=zplt_cen;
    meshi->xplt_orig=xp2;
    meshi->yplt_orig=yp2;
    meshi->zplt_orig=zp2;
    meshi->ibar=ibartemp;
    meshi->jbar=jbartemp;
    meshi->kbar=kbartemp;
    meshi->plotx=ibartemp/2;
    meshi->ploty=jbartemp/2;
    meshi->plotz=kbartemp/2;
  }
  if(setPDIM==0&&roomdefined==1){
    mesh *meshi;

    meshi=meshinfo;
    meshi->xyz_bar0[XXX]=xbar0;
    meshi->xyz_bar[XXX] =xbar;
    meshi->xcen=(xbar+xbar0)/2.0;
    meshi->xyz_bar0[YYY]=ybar0;
    meshi->xyz_bar[YYY] =ybar;
    meshi->ycen=(ybar+ybar0)/2.0;
    meshi->xyz_bar0[ZZZ]=zbar0;
    meshi->xyz_bar[ZZZ] =zbar;
    meshi->zcen=(zbar+zbar0)/2.0;
  }

  if(ndeviceinfo>0){
    if(NewMemory((void **)&deviceinfo,ndeviceinfo*sizeof(devicedata))==0)return 2;
    devicecopy=deviceinfo;;
  }
  ndeviceinfo=0;
  rewind(stream1);
  if(stream2!=NULL)rewind(stream2);
  stream=stream1;
  printf("%s",_("   pass 3 started"));
  printf("\n");

  /* 
   ************************************************************************
   ************************ start of pass 3 ****************************** 
   ************************************************************************
 */

  for(;;){
    if(feof(stream)!=0){
      BREAK;
    }
    if(fgets(buffer,255,stream)==NULL){
      BREAK;
    }
    trim(buffer);
    if(strncmp(buffer," ",1)==0||buffer[0]==0)continue;
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ DEVICE +++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    DEVICE
    label
    x y z xn yn zn state nparams ntextures 
    p0 p1 ... p5
    p6 ...    p11
    texturefile1
    ...
    texturefilen

    */
    if(
      (match(buffer,"DEVICE") == 1)&&
      (match(buffer,"DEVICE_ACT") != 1)
      ){
      devicedata *devicei;

      devicei = deviceinfo + ndeviceinfo;
      parse_device_keyword(stream,devicei);
      CheckMemory;
      ndeviceinfo++;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ THCP ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"THCP") == 1){
      mesh *meshi;
      float normdenom;
      char *device_label;

      if(ioffset==0)ioffset=1;
      meshi=meshinfo + ioffset - 1;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&tempval);
      if(tempval<0)tempval=0;
      meshi->ntc=tempval;
      ntc_total += meshi->ntc;
      hasSensorNorm=0;
      if(meshi->ntc>0){
        for(nn=0;nn<meshi->ntc;nn++){
          float *xyz, *xyznorm;
          fgets(buffer,255,stream);

          xyz = devicecopy->xyz;
          xyznorm = devicecopy->xyznorm;
          xyz[0]=0.0;
          xyz[1]=0.0;
          xyz[2]=0.0;
          xyznorm[0]=0.0;
          xyznorm[1]=0.0;
          xyznorm[2]=-1.0;
          device_label=get_device_label(buffer);
          sscanf(buffer,"%f %f %f %f %f %f",xyz,xyz+1,xyz+2,xyznorm,xyznorm+1,xyznorm+2);
          normdenom=0.0;
          normdenom+=xyznorm[0]*xyznorm[0];
          normdenom+=xyznorm[1]*xyznorm[1];
          normdenom+=xyznorm[2]*xyznorm[2];
          if(normdenom>0.1){
            hasSensorNorm=1;
            normdenom=sqrt(normdenom);
            xyznorm[0]/=normdenom;
            xyznorm[1]/=normdenom;
            xyznorm[2]/=normdenom;
          }
          if(device_label==NULL){
            if(isZoneFireModel==1){
              devicecopy->object = get_SVOBJECT_type("target",thcp_object_backup);
            }
            else{
              devicecopy->object = get_SVOBJECT_type("thermoc4",thcp_object_backup);
            }
          }
          else{
            devicecopy->object = get_SVOBJECT_type(device_label,thcp_object_backup);
          }
          get_elevaz(xyznorm,&devicecopy->dtheta,devicecopy->rotate_axis,NULL);
    
          init_device(devicecopy,xyz,xyznorm,0,0,NULL,NULL);
          devicecopy->prop=NULL;

          devicecopy++;
          ndeviceinfo++;
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SPRK ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SPRK") == 1){
      mesh *meshi;
      char *device_label;
      meshi=meshinfo + ioffset - 1;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&tempval);
      if(tempval<0)tempval=0;
      meshi->nspr=tempval;
      nspr_total += meshi->nspr;
      if(meshi->nspr>0){
        FREEMEMORY(meshi->xspr); FREEMEMORY(meshi->yspr); FREEMEMORY(meshi->zspr); FREEMEMORY(meshi->tspr);
        if(NewMemory((void **)&meshi->xspr,meshi->nspr*sizeof(float))==0||
           NewMemory((void **)&meshi->yspr,meshi->nspr*sizeof(float))==0||
           NewMemory((void **)&meshi->zspr,meshi->nspr*sizeof(float))==0||
           NewMemory((void **)&meshi->tspr,meshi->nspr*sizeof(float))==0||
           NewMemory((void **)&meshi->xsprplot,meshi->nspr*sizeof(float))==0||
           NewMemory((void **)&meshi->ysprplot,meshi->nspr*sizeof(float))==0||
           NewMemory((void **)&meshi->zsprplot,meshi->nspr*sizeof(float))==0)return 2;
        for(nn=0;nn<meshi->nspr;nn++){meshi->tspr[nn]=99999.;}
        xsprcopy=meshi->xspr;
        ysprcopy=meshi->yspr;
        zsprcopy=meshi->zspr;
        for(nn=0;nn<meshi->nspr;nn++){
          float *xyznorm;
          float normdenom;

          fgets(buffer,255,stream);
          xyznorm = devicecopy->xyznorm;
          xyznorm[0]=0.0;
          xyznorm[1]=0.0;
          xyznorm[2]=-1.0;
          device_label=get_device_label(buffer);
          sscanf(buffer,"%f %f %f %f %f %f",xsprcopy,ysprcopy,zsprcopy,xyznorm,xyznorm+1,xyznorm+2);
          devicecopy->act_time=-1.0;
          devicecopy->type = DEVICE_SPRK;
          devicecopy->xyz[0]=*xsprcopy;
          devicecopy->xyz[1]=*ysprcopy;
          devicecopy->xyz[2]=*zsprcopy;
          normdenom=0.0;
          normdenom+=xyznorm[0]*xyznorm[0];
          normdenom+=xyznorm[1]*xyznorm[1];
          normdenom+=xyznorm[2]*xyznorm[2];
          normdenom=sqrt(normdenom);
          if(normdenom>0.001){
            xyznorm[0]/=normdenom;
            xyznorm[1]/=normdenom;
            xyznorm[2]/=normdenom;
          }
          if(device_label==NULL){
            devicecopy->object = get_SVOBJECT_type("sprinkler_upright",sprinkler_upright_object_backup);
          }
          else{
            devicecopy->object = get_SVOBJECT_type(device_label,sprinkler_upright_object_backup);
          }
          get_elevaz(xyznorm,&devicecopy->dtheta,devicecopy->rotate_axis,NULL);
    
          init_device(devicecopy,NULL,xyznorm,0,0,NULL,NULL);

          devicecopy++;
          ndeviceinfo++;

          xsprcopy++; ysprcopy++; zsprcopy++;
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ HEAT ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"HEAT") == 1){
      mesh *meshi;
      char *device_label;

      meshi=meshinfo + ioffset - 1;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&tempval);
      if(tempval<0)tempval=0;
      meshi->nheat=tempval;
      nheat_total += meshi->nheat;
      if(meshi->nheat>0){
        FREEMEMORY(meshi->xheat); FREEMEMORY(meshi->yheat); FREEMEMORY(meshi->zheat); FREEMEMORY(meshi->theat);
        FREEMEMORY(meshi->xheatplot); FREEMEMORY(meshi->yheatplot); FREEMEMORY(meshi->zheatplot);
        if(NewMemory((void **)&meshi->xheat,meshi->nheat*sizeof(float))==0||
           NewMemory((void **)&meshi->yheat,meshi->nheat*sizeof(float))==0||
           NewMemory((void **)&meshi->zheat,meshi->nheat*sizeof(float))==0||
           NewMemory((void **)&meshi->theat,meshi->nheat*sizeof(float))==0||
           NewMemory((void **)&meshi->xheatplot,meshi->nheat*sizeof(float))==0||
           NewMemory((void **)&meshi->yheatplot,meshi->nheat*sizeof(float))==0||
           NewMemory((void **)&meshi->zheatplot,meshi->nheat*sizeof(float))==0)return 2;
        for(nn=0;nn<meshi->nheat;nn++){
          meshi->theat[nn]=99999.;
        }
        xheatcopy=meshi->xheat;
        yheatcopy=meshi->yheat;
        zheatcopy=meshi->zheat;
        for(nn=0;nn<meshi->nheat;nn++){
          float *xyznorm;
          float normdenom;
          fgets(buffer,255,stream);
          xyznorm=devicecopy->xyznorm;
          xyznorm[0]=0.0;
          xyznorm[1]=0.0;
          xyznorm[2]=-1.0;
          device_label=get_device_label(buffer);
          sscanf(buffer,"%f %f %f %f %f %f",xheatcopy,yheatcopy,zheatcopy,xyznorm,xyznorm+1,xyznorm+2);
          devicecopy->type = DEVICE_HEAT;
          devicecopy->act_time=-1.0;
          devicecopy->xyz[0]=*xheatcopy;
          devicecopy->xyz[1]=*yheatcopy;
          devicecopy->xyz[2]=*zheatcopy;
          normdenom=0.0;
          normdenom+=xyznorm[0]*xyznorm[0];
          normdenom+=xyznorm[1]*xyznorm[1];
          normdenom+=xyznorm[2]*xyznorm[2];
          normdenom=sqrt(normdenom);
          if(normdenom>0.001){
            xyznorm[0]/=normdenom;
            xyznorm[1]/=normdenom;
            xyznorm[2]/=normdenom;
          }
          if(device_label==NULL){
            devicecopy->object = get_SVOBJECT_type("heat_detector",heat_detector_object_backup);
          }
          else{
            devicecopy->object = get_SVOBJECT_type(device_label,heat_detector_object_backup);
          }
          get_elevaz(xyznorm,&devicecopy->dtheta,devicecopy->rotate_axis,NULL);

          init_device(devicecopy,NULL,xyznorm,0,0,NULL,NULL);
          devicecopy->prop=NULL;

          devicecopy++;
          ndeviceinfo++;
          xheatcopy++; yheatcopy++; zheatcopy++;

        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SMOD ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SMOD") == 1){
      float xyz[3];
      int sdnum;
      char *device_label;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&sdnum);
      if(sdnum<0)sdnum=0;
      for(nn=0;nn<sdnum;nn++){
        float *xyznorm;
        float normdenom;

        xyznorm = devicecopy->xyznorm;
        xyznorm[0]=0.0;
        xyznorm[1]=0.0;
        xyznorm[2]=-1.0;
        device_label=get_device_label(buffer);
        fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f %f %f %f",xyz,xyz+1,xyz+2,xyznorm,xyznorm+1,xyznorm+2);
        devicecopy->type = DEVICE_SMOKE;
        devicecopy->act_time=-1.0;
        devicecopy->xyz[0]=xyz[0];
        devicecopy->xyz[1]=xyz[1];
        devicecopy->xyz[2]=xyz[2];
        normdenom=0.0;
        normdenom+=xyznorm[0]*xyznorm[0];
        normdenom+=xyznorm[1]*xyznorm[1];
        normdenom+=xyznorm[2]*xyznorm[2];
        normdenom=sqrt(normdenom);
        if(normdenom>0.001){
          xyznorm[0]/=normdenom;
          xyznorm[1]/=normdenom;
          xyznorm[2]/=normdenom;
        }
        if(device_label==NULL){
          devicecopy->object = get_SVOBJECT_type("smoke_detector",smoke_detector_object_backup);
        }
        else{
          devicecopy->object = get_SVOBJECT_type(device_label,smoke_detector_object_backup);
        }
        get_elevaz(xyznorm,&devicecopy->dtheta,devicecopy->rotate_axis,NULL);
    
        init_device(devicecopy,xyz,xyznorm,0,0,NULL,NULL);

        devicecopy++;
        ndeviceinfo++;

      }
      continue;
    }
  }
  /* 
   ************************************************************************
   ************************ end of pass 3 ****************************** 
   ************************************************************************
 */

  // look for DEVICE entries in "experimental" spread sheet files
  
  if(ncsvinfo>0){
    int *nexp_devices=NULL;
    devicedata *devicecopy2;

    NewMemory((void **)&nexp_devices,ncsvinfo*sizeof(int));
    for(i=0;i<ncsvinfo;i++){
      csvdata *csvi;

      csvi = csvinfo + i;
      if(csvi->type==CSVTYPE_EXT){
        nexp_devices[i] = get_ndevices(csvi->file);
        ndeviceinfo_exp += nexp_devices[i];
      }
    }
    if(ndeviceinfo>0){
      if(ndeviceinfo_exp>0){
        ResizeMemory((void **)&deviceinfo,(ndeviceinfo_exp+ndeviceinfo)*sizeof(devicedata));
      }
    }
    else{
      if(ndeviceinfo_exp>0)NewMemory((void **)&deviceinfo,ndeviceinfo_exp*sizeof(devicedata));
    }
    devicecopy2 = deviceinfo+ndeviceinfo;
    ndeviceinfo+=ndeviceinfo_exp;

    for(i=0;i<ncsvinfo;i++){
      csvdata *csvi;

      csvi = csvinfo + i;
      if(csvi->type==CSVTYPE_EXT){
        read_device_header(csvi->file,devicecopy2,nexp_devices[i]);
        devicecopy2 += nexp_devices[i];
      }
    }
    FREEMEMORY(nexp_devices);
    devicecopy2=deviceinfo;
  }

  // define texture data structures by constructing a list of unique file names from surfinfo and devices   

  update_device_textures();
  if(nsurfinfo>0||ndevice_texture_list>0){
    if(NewMemory((void **)&textureinfo,(nsurfinfo+ndevice_texture_list)*sizeof(texture))==0)return 2;
  }
  init_textures();

/* 
    Initialize blockage labels and blockage surface labels

    Define default surface for each block.
    Define default vent surface for each block.
  
  */

  surfacedefault=&sdefault;
  for(k=0;k<nsurfinfo;k++){
    if(strcmp(surfacedefaultlabel,surfinfo[k].surfacelabel)==0){
      surfacedefault=surfinfo+k;
      break;
    }
  }
  vent_surfacedefault=&v_surfacedefault;
  for(k=0;k<nsurfinfo;k++){
    if(strcmp(vent_surfacedefault->surfacelabel,surfinfo[k].surfacelabel)==0){
      vent_surfacedefault=surfinfo+k;
      break;
    }
  }

  exterior_surfacedefault=&e_surfacedefault;
  for(k=0;k<nsurfinfo;k++){
    if(strcmp(exterior_surfacedefault->surfacelabel,surfinfo[k].surfacelabel)==0){
      exterior_surfacedefault=surfinfo+k;
      break;
    }
  }

  nvents=0;
  itrnx=0, itrny=0, itrnz=0, igrid=0, ipdim=0, iobst=0, ivent=0;
  ioffset=0;
  npartclassinfo=0;
  if(noffset==0)ioffset=1;

/* 
   ************************************************************************
   ************************ start of pass 4 ********************************* 
   ************************************************************************
 */

  rewind(stream1);
  if(stream2!=NULL)rewind(stream2);
  stream=stream1;
  printf("%s",_("   pass 3 "));
  printf("%s",_("completed"));
  printf("\n");
  printf("%s",_("   pass 4 started"));
  printf("\n");
  startpass=1;

  for(;;){
    if(feof(stream)!=0){
      BREAK;
    }

    if(nvent==0){
      nvent=0;
      strcpy(buffer,"VENT");
    }
    else{
      if(startpass==1&&noGRIDpresent==1){
        strcpy(buffer,"GRID");
        startpass=0;
      }
      else{
        if(fgets(buffer,255,stream)==NULL){
          BREAK;
        }
        if(strncmp(buffer," ",1)==0||buffer[0]==0)continue;
      }
    }
    CheckMemory;
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++ CLASS_OF_PARTICLES +++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"CLASS_OF_PARTICLES") == 1||
       match(buffer,"CLASS_OF_HUMANS") == 1){
      part5class *partclassi;
      char *device_ptr;
      char *prop_id;
      char prop_buffer[255];

      partclassi = partclassinfo + npartclassinfo;
      partclassi->kind=PARTICLES;
      if(match(buffer,"CLASS_OF_HUMANS") == 1)partclassi->kind=HUMANS;
      fgets(buffer,255,stream);

      get_labels(buffer,partclassi->kind,&device_ptr,&prop_id,prop_buffer);
      partclassi->prop=get_prop_id(prop_id);
      update_partclass_depend(partclassi);

      npartclassinfo++;
      continue;
    }
    
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ HRRPUVCUT ++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"HRRPUVCUT") == 1){
      int nhrrpuvcut;
      float hrrpuvcut;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&nhrrpuvcut);
      if(nhrrpuvcut>=1){
        fgets(buffer,255,stream);
        sscanf(buffer,"%f",&hrrpuvcut);
        for(i=0;i<nmeshes;i++){
          mesh *meshi;

          meshi=meshinfo+i;
          meshi->hrrpuv_cutoff=hrrpuvcut;
        }
        global_hrrpuv_cutoff=meshinfo->hrrpuv_cutoff;
        for(i=1;i<nhrrpuvcut;i++){
          mesh *meshi;

          fgets(buffer,255,stream);
          if(i>=nmeshes)continue;
          sscanf(buffer,"%f",&hrrpuvcut);
          meshi=meshinfo+i;
          meshi->hrrpuv_cutoff=hrrpuvcut;
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ OFFSET ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"OFFSET") == 1){
      mesh *meshi;

      ioffset++;
      meshi=meshinfo+ioffset-1;
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",meshi->offset,meshi->offset+1,meshi->offset+2);
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ VENTGEOM ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"VENTGEOM")==1||match(buffer,"VFLOWGEOM")==1){
      int zonevent_orien=0;
      int hvent_type=0;
      zvent *zvi;
      float vent_area;
      int roomfrom, roomto, face;
      roomdata *roomi;
      float color[4];

      nzvents++;
      zvi = zventinfo + nzvents - 1;
      if(match(buffer,"VFLOWGEOM")==1)zonevent_orien=1;
      zvi->vent_orien=zonevent_orien;
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      color[0]=1.0;
      color[1]=0.0;
      color[2]=1.0;
      color[3]=1.0;
      if(zonevent_orien==0){
        nzhvents++;
        sscanf(buffer,"%i %i %i %f %f %f %f %f %f %f",
          &roomfrom,&roomto, &face,&width,&ventoffset,&bottom,&top,
          color,color+1,color+2
          );

        zvi->area=width*(top-bottom);
        
        if(roomfrom<1||roomfrom>nrooms)roomfrom=nrooms+1;
        roomi = roominfo + roomfrom-1;
        zvi->room1 = roomi;

        if(roomto<1||roomto>nrooms)roomto=nrooms+1;
        zvi->room2=roominfo+roomto-1;

        zvi->dir=face;
        zvi->z1=roomi->z0+bottom;
        zvi->z2=roomi->z0+top;
        zvi->face=face;
        switch (face){
        case 1:
          zvi->yy=roomi->y0;
          zvi->x1=roomi->x0+ventoffset;
          zvi->x2=roomi->x0+ventoffset+width;
          break;
        case 2:
          zvi->yy=roomi->x1;
          zvi->x1=roomi->y0+ventoffset;
          zvi->x2=roomi->y0+ventoffset+width;
          break;
        case 3:
          zvi->yy=roomi->y1;
          zvi->x1=roomi->x0+ventoffset;
          zvi->x2=roomi->x0+ventoffset+width;
          break;
        case 4:
          zvi->yy=roomi->x0;
          zvi->x1=roomi->y0+ventoffset;
          zvi->x2=roomi->y0+ventoffset+width;
          break;
        default:
          ASSERT(FFALSE);
        }
      }
      else{
        float ventside;
        float xcen, ycen;
        int r_from, r_to;

        nzvvents++;
        sscanf(buffer,"%i %i %i %f %i %f %f %f",
          &r_from,&r_to,&face,&vent_area,&hvent_type,
          color,color+1,color+2
          );
        zvi->dir=face;
        roomfrom=r_from;
        roomto=r_to;
        if(roomfrom<1||roomfrom>nrooms){
          roomfrom=r_to;
          roomto=r_from;
          if(roomfrom<1||roomfrom>nrooms){
            roomfrom=1;
          }
        }
        roomi = roominfo + roomfrom - 1;
        if(vent_area<0.0)vent_area=-vent_area;
        ventside=sqrt(vent_area);
        xcen = (roomi->x0+roomi->x1)/2.0;
        ycen = (roomi->y0+roomi->y1)/2.0;
        if(face==5){
          zvi->zz=roomi->z0;
        }
        else{
          zvi->zz=roomi->z1;
        }
        switch (hvent_type){
        case 1:
        case 2:
          zvi->x1=xcen-ventside/2.0;
          zvi->x2=xcen+ventside/2.0;
          zvi->y1=ycen-ventside/2.0;
          zvi->y2=ycen+ventside/2.0;
          break;
        default:
          ASSERT(FFALSE);
          break;
        }

        zvi->vent_type=hvent_type;
        zvi->area=vent_area;
        zvi->face=face;
      }
      zvi->color=getcolorptr(color);
      CheckMemory;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TITLE1 ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"TITLE1")==1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      bufferptr=trim_string(buffer);
      strcpy(TITLE1,bufferptr);
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TITLE2 ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"TITLE2")==1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      bufferptr=trim_string(buffer);
      strcpy(TITLE2,bufferptr);
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ FIRE ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"FIRE")==1){
      firedata *firei;

      ifire++;
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      firei = fireinfo + ifire - 1;
      sscanf(buffer,"%i %f %f %f",&roomnumber,&firei->x,&firei->y,&firei->z);
      if(roomnumber>=1&&roomnumber<=nrooms){
        roomdata *roomi;
  
        roomi = roominfo + roomnumber - 1;
        firei->valid=1;
        firei->roomnumber=roomnumber;
        firei->absx=roomi->x0+firei->x;
        firei->absy=roomi->y0+firei->y;
        firei->absz=roomi->z0+firei->z;
      }
      else{
        firei->valid=0;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ PDIM ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"PDIM") == 1){
      mesh *meshi;
      float *meshrgb;

      ipdim++;
      meshi=meshinfo+ipdim-1;
      meshrgb = meshi->meshrgb;

      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f %f %f %f %f %f %f",&xbar0,&xbar,&ybar0,&ybar,&zbar0,&zbar,meshrgb,meshrgb+1,meshrgb+2);

      if(meshrgb[0]!=0.0||meshrgb[1]!=0.0||meshrgb[2]!=0.0){
        meshi->meshrgb_ptr=meshi->meshrgb;
      }
      else{
        meshi->meshrgb_ptr=NULL;
      }

      meshi->xyz_bar0[XXX]=xbar0;
      meshi->xyz_bar[XXX] =xbar;
      meshi->xcen=(xbar+xbar0)/2.0;
      meshi->xyz_bar0[YYY]=ybar0;
      meshi->xyz_bar[YYY] =ybar;
      meshi->ycen =(ybar+ybar0)/2.0;
      meshi->xyz_bar0[ZZZ]=zbar0;
      meshi->xyz_bar[ZZZ] =zbar;
      meshi->zcen =(zbar+zbar0)/2.0;
      if(ntrnx==0){
        for(nn=0;nn<=meshi->ibar;nn++){
          meshi->xplt[nn]=xbar0+(float)nn*(xbar-xbar0)/(float)meshi->ibar;
        }
      }
      if(ntrny==0){
        for(nn=0;nn<=meshi->jbar;nn++){
          meshi->yplt[nn]=ybar0+(float)nn*(ybar-ybar0)/(float)meshi->jbar;
        }
      }
      if(ntrnz==0){
        for(nn=0;nn<=meshi->kbar;nn++){
          meshi->zplt[nn]=zbar0+(float)nn*(zbar-zbar0)/(float)meshi->kbar;
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TRNX ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"TRNX")==1){
      float *xpltcopy;

      itrnx++;
      xpltcopy=meshinfo[itrnx-1].xplt;
      ibartemp=meshinfo[itrnx-1].ibar;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&idummy);
      for(nn=0;nn<idummy;nn++){
        fgets(buffer,255,stream);
      }
      for(nn=0;nn<=ibartemp;nn++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i %f",&idummy,xpltcopy);xpltcopy++;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TRNY ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"TRNY") == 1){
      float *ypltcopy;

      itrny++;
      ypltcopy=meshinfo[itrny-1].yplt;
      jbartemp=meshinfo[itrny-1].jbar;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&idummy);
      for(nn=0;nn<idummy;nn++){
        fgets(buffer,255,stream);
      }
      for(nn=0;nn<=jbartemp;nn++){
//        if(jbartemp==2&&nn==2)continue;
        fgets(buffer,255,stream);
        sscanf(buffer,"%i %f",&idummy,ypltcopy);
        ypltcopy++;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TRNZ ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"TRNZ") == 1){
      float *zpltcopy;

      itrnz++;
      zpltcopy=meshinfo[itrnz-1].zplt;
      kbartemp=meshinfo[itrnz-1].kbar;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&idummy);
      for(nn=0;nn<idummy;nn++){
        fgets(buffer,255,stream);
      }
      for(nn=0;nn<=kbartemp;nn++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i %f",&idummy,zpltcopy);zpltcopy++;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TREE ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    /*
typedef struct {
  float xyz[3];
  float trunk_diam;
  float tree_height;
  float base_diam;
  float base_height;
*/
    if(match(buffer,"TREE") == 1){
      fgets(buffer,255,stream);
      if(ntreeinfo!=0)continue;
      sscanf(buffer,"%i",&ntreeinfo);
      if(ntreeinfo>0){
        NewMemory((void **)&treeinfo,sizeof(treedata)*ntreeinfo);
        for(i=0;i<ntreeinfo;i++){
          treedata *treei;
          float *xyz;

          treei = treeinfo + i;
          treei->time_char=-1.0;
          treei->time_complete=-1.0;
          xyz = treei->xyz;
          fgets(buffer,255,stream);
          sscanf(buffer,"%f %f %f %f %f %f %f",
            xyz,xyz+1,xyz+2,
            &treei->trunk_diam,&treei->tree_height,
            &treei->base_diam,&treei->base_height);
        }
      }
      continue;
    }
    if(match(buffer,"TREESTATE") ==1){
      int tree_index, tree_state;
      float tree_time;
      treedata *treei;

      if(ntreeinfo==0)continue;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %f",&tree_index,&tree_state,&tree_time);
      if(tree_index>=1&&tree_index<=ntreeinfo){
        treei = treeinfo + tree_index - 1;
        if(tree_state==1){
          treei->time_char = tree_time;
        }
        else if(tree_state==2){
          treei->time_complete = tree_time;
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ OBST ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"OBST") == 1){
      mesh *meshi;
      propdata *prop;
      char *proplabel;
      int n_blocks=0;
      int n_blocks_normal=0;
      unsigned char *is_block_terrain=NULL;
      int iblock;

      CheckMemoryOff;
      iobst++;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n_blocks);

      meshi=meshinfo+iobst-1;
      if(n_blocks<=0)n_blocks=0;
      meshi->nbptrs=n_blocks;
      n_blocks_normal=n_blocks;
      if(n_blocks==0)continue;

      if(autoterrain==1){
        is_block_terrain=meshi->is_block_terrain;
        n_blocks_normal=0;
        for(iblock=0;iblock<n_blocks;iblock++){
          if(is_block_terrain[iblock]==0)n_blocks_normal++;
        }
        meshi->nbptrs=n_blocks_normal;
      }
      meshi->blockageinfoptrs=NULL;
      if(n_blocks_normal>0){
        NewMemory((void **)&meshi->blockageinfoptrs,sizeof(blockagedata *)*n_blocks_normal);
      }

      ntotal_blockages+=n_blocks_normal;
      nn=-1;
      for(iblock=0;iblock<n_blocks;iblock++){
        int s_num[6];
        blockagedata *bc;

        if(autoterrain==1&&meshi->is_block_terrain!=NULL&&meshi->is_block_terrain[iblock]==1){
          fgets(buffer,255,stream);
          continue;
        }
        nn++;
        meshi->blockageinfoptrs[nn]=NULL;
        NewMemory((void **)&meshi->blockageinfoptrs[nn],sizeof(blockagedata));
        bc=meshi->blockageinfoptrs[nn];
        initobst(bc,surfacedefault,nn+1,iobst-1);
        fgets(buffer,255,stream);
        trim(buffer);
        for(i=0;i<6;i++){
          s_num[i]=-1;
        }
        proplabel=strchr(buffer,'%');
        prop=NULL;
        if(proplabel!=NULL){
          proplabel++;
          trim(proplabel);
          proplabel = trim_front(proplabel);
          for(i=0;i<npropinfo;i++){
            propdata *propi;

            propi = propinfo + i;
            if(STRCMP(proplabel,propi->label)==0){
              prop = propi;
              propi->inblockage=1;
              break;
            }
          }
        }
        bc->prop=prop;
        {
          float t_origin[3];
          float *xyzEXACT;

          t_origin[0]=texture_origin[0];
          t_origin[1]=texture_origin[1];
          t_origin[2]=texture_origin[2];
          xyzEXACT = bc->xyzEXACT;
          sscanf(buffer,"%f %f %f %f %f %f %i %i %i %i %i %i %i %f %f %f",
            xyzEXACT,xyzEXACT+1,xyzEXACT+2,xyzEXACT+3,xyzEXACT+4,xyzEXACT+5,
            &(bc->id),s_num+DOWN_X,s_num+UP_X,s_num+DOWN_Y,s_num+UP_Y,s_num+DOWN_Z,s_num+UP_Z,
            t_origin,t_origin+1,t_origin+2);
          bc->xmin=xyzEXACT[0];
          bc->xmax=xyzEXACT[1];
          bc->ymin=xyzEXACT[2];
          bc->ymax=xyzEXACT[3];
          bc->zmin=xyzEXACT[4];
          bc->zmax=xyzEXACT[5];
          bc->texture_origin[0]=t_origin[0];
          bc->texture_origin[1]=t_origin[1];
          bc->texture_origin[2]=t_origin[2];
          if(bc->id<0){
            bc->changed=1;
            bc->id=-bc->id;
          }
        }

        /* define block label */

        sprintf(buffer,"**blockage %i",bc->id);
        len=strlen(buffer);
        ResizeMemory((void **)&bc->label,(len+1)*sizeof(char));
        strcpy(bc->label,buffer);

        for(i=0;i<6;i++){
          surfdata *surfi;

          if(surfinfo==NULL||s_num[i]<0||s_num[i]>=nsurfinfo)continue;
          surfi=surfinfo+s_num[i];
          bc->surf[i]=surfi;
        }
        for(i=0;i<6;i++){
          bc->surf[i]->used_by_obst=1;
        }
        setsurfaceindex(bc);
      }

      nn=-1;
      for(iblock=0;iblock<n_blocks;iblock++){
        blockagedata *bc;

        if(autoterrain==1&&meshi->is_block_terrain!=NULL&&meshi->is_block_terrain[iblock]==1){
          fgets(buffer,255,stream);
          continue;
        }
        nn++;

        bc=meshi->blockageinfoptrs[nn];
        colorindex=-1;
        blocktype=-1;

        /* 
        OBST format:
        i1 i2 j1 j2 k1 k2 colorindex blocktype r g b : ignore rgb if blocktype != -3
        
        int colorindex, blocktype;
        colorindex: -1 default color
                    -2 invisible
                    -3 use r g b color
                    >=0 color/color2/texture index
        blocktype: 0 regular block
                   2 outline
                   3 smoothed block
                   (note: if blocktype&8 == 8 then this is a "terrain" blockage
                         if so then subtract 8 and set bc->is_wuiblock=1)
        r g b           colors used if colorindex==-3
        */

        fgets(buffer,255,stream);
        ijk = bc->ijk;
        sscanf(buffer,"%i %i %i %i %i %i %i %i",
          ijk,ijk+1,ijk+2,ijk+3,ijk+4,ijk+5,
          &colorindex,&blocktype);
        if(blocktype>0&&(blocktype&8)==8){
          bc->is_wuiblock=1;
          blocktype -= 8;
        }

        /* custom color */
        
        if(colorindex==0||colorindex==7)colorindex=-3;

        if(colorindex==-3){
          ijk = bc->ijk;

          s_color[0]=-1.0;
          s_color[1]=-1.0;
          s_color[2]=-1.0;
          s_color[3]=1.0;

          sscanf(buffer,"%i %i %i %i %i %i %i %i %f %f %f %f",
            ijk,ijk+1,ijk+2,ijk+3,ijk+4,ijk+5,
            &colorindex,&blocktype,s_color,s_color+1,s_color+2,s_color+3);
          if(blocktype>0&&(blocktype&8)==8){
            bc->is_wuiblock=1;
            blocktype -= 8;
          }
          if(s_color[3]<0.999){
            bc->transparent=1;
          }
          if(colorindex==0||colorindex==7){
            switch (colorindex){
            case 0:
              s_color[0]=1.0;
              s_color[1]=1.0;
              s_color[2]=1.0;
              break;
            case 7:
              s_color[0]=0.0;
              s_color[1]=0.0;
              s_color[2]=0.0;
              break;
            default:
              ASSERT(FFALSE);
              break;
            }
            colorindex=-3;
          }
          if(s_color[0]>=0.0&&s_color[1]>=0.0&&s_color[2]>=0.0){
            bc->color=getcolorptr(s_color);
          }
          bc->nnodes=(ijk[1]+1-ijk[0])*(ijk[3]+1-ijk[2])*(ijk[5]+1-ijk[4]);
          bc->useblockcolor=1;
        }
        else{
          if(colorindex>=0){
            bc->color = getcolorptr(rgb[nrgb+colorindex]);
            bc->useblockcolor=1;
            bc->usecolorindex=1;
            bc->colorindex=colorindex;
            updateindexcolors=1;
          }
        }

        bc->dup=0;
        bc->colorindex=colorindex;
        bc->type=blocktype;

        if(colorindex==COLOR_INVISIBLE){
          bc->type=BLOCK_hidden;
//          bc->del=1;
          bc->invisible=1;
        }
        if(bc->useblockcolor==0){
          bc->color=bc->surf[0]->color;
        }
        else{
          if(colorindex==-1){
            updateindexcolors=1;
          }
        }
        if(bc->type==BLOCK_smooth){
          ntotal_smooth_blockages++;
          use_menusmooth=1;
        }

      }
      CheckMemoryOn;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ VENT ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
        /* 
        new VENT format:
        i1 i2 j1 j2 k1 k2 ventindex venttype r g b a
        
        int ventindex, venttype;
        vent index: -99 or 99 : use default color
                     +n or -n : use n'th pallette color
                    < 0       : DO NOT draw boundary file over this vent
                    > 0       : DO draw boundary file over this vent
        vent type: 0 solid surface
                   2 outline
                  -2 hidden 

        r g b           colors - only specify if you wish to over-ride surface or default
        */

    if(match(buffer,"VENT") == 1){
      mesh *meshi;
      ventdata *vinfo;

      ivent++;
      meshi=meshinfo+ivent-1;
      xplttemp=meshi->xplt;
      yplttemp=meshi->yplt;
      zplttemp=meshi->zplt;
      if(nvent==0){
        strcpy(buffer,"0 0");
        nvent=1;
      }
      else{
        fgets(buffer,255,stream);
      }
      ndummyvents=0;
      sscanf(buffer,"%i %i",&nvents,&ndummyvents);
      if(ndummyvents!=0)dummyvents=1;
      meshi->nvents=nvents;
      meshi->ndummyvents=ndummyvents;
      vinfo=NULL;
      meshi->ventinfo=vinfo;
      if(NewMemory((void **)&vinfo,(nvents+12)*sizeof(ventdata))==0)return 2;
      meshi->ventinfo=vinfo;

      for(nn=0;nn<nvents+12;nn++){
        int s_num[6];
        ventdata *vi;

        vi=vinfo+nn;
        vi->transparent=0;
        vi->useventcolor=0;
        vi->usecolorindex=0;
        vi->nshowtime=0;
        vi->isOpenvent=0;
		    vi->hideboundary=0;
        vi->surf[0]=vent_surfacedefault;
        vi->textureinfo[0]=NULL;
        vi->texture_origin[0]=texture_origin[0];
        vi->texture_origin[1]=texture_origin[1];
        vi->texture_origin[2]=texture_origin[2];
        vi->colorindex=-1;
        if(nn>nvents-ndummyvents-1&&nn<nvents){
          vi->dummy=1;
        }
        else{
          vi->dummy=0;
        }
        s_num[0]=-1;
        if(nn<nvents){
          {
            float t_origin[3];
            t_origin[0]=texture_origin[0];
            t_origin[1]=texture_origin[1];
            t_origin[2]=texture_origin[2];
            fgets(buffer,255,stream);
            sscanf(buffer,"%f %f %f %f %f %f %i %i %f %f %f",
                   &vi->xmin,&vi->xmax,&vi->ymin,&vi->ymax,&vi->zmin,&vi->zmax,
                   &vi->id,s_num,t_origin,t_origin+1,t_origin+2);
            vi->texture_origin[0]=t_origin[0];
            vi->texture_origin[1]=t_origin[1];
            vi->texture_origin[2]=t_origin[2];
          }
        }
        else{
          vi->xmin=meshi->xyz_bar0[XXX]+meshi->offset[XXX];
          vi->xmax=meshi->xyz_bar[XXX] +meshi->offset[XXX];
          vi->ymin=meshi->xyz_bar0[YYY]+meshi->offset[YYY];
          vi->ymax=meshi->xyz_bar[YYY] +meshi->offset[YYY];
          vi->zmin=meshi->xyz_bar0[ZZZ]+meshi->offset[ZZZ];
          vi->zmax=meshi->xyz_bar[ZZZ] +meshi->offset[ZZZ];
          s_num[0]=-1;
          switch (nn-nvents){
          case DOWN_Y:
          case DOWN_Y+6:
            vi->ymax=vi->ymin;
            break;
          case UP_X:
          case UP_X+6:
            vi->xmin=vi->xmax;
            break;
          case UP_Y:
          case UP_Y+6:
            vi->ymin=vi->ymax;
            break;
          case DOWN_X:
          case DOWN_X+6:
            vi->xmax=vi->xmin;
            break;
          case DOWN_Z:
          case DOWN_Z+6:
            vi->zmax=vi->zmin;
            break;
          case UP_Z:
          case UP_Z+6:
            vi->zmin=vi->zmax;
            break;
          default:
            ASSERT(FFALSE);
            break;
          }
          if(nn>=nvents+6){
            vi->surf[0]=exterior_surfacedefault;
          }
        }
        if(surfinfo!=NULL&&s_num[0]>=0&&s_num[0]<nsurfinfo){
          vi->surf[0]=surfinfo+s_num[0];
          if(vi->surf[0]!=NULL&&strncmp(vi->surf[0]->surfacelabel,"OPEN",4)==0){
            vi->isOpenvent=1;
          }
          vi->surf[0]->used_by_vent=1;
        }
      }
      for(nn=0;nn<nvents+12;nn++){
        ventdata *vi;

        vi = vinfo+nn;
        vi->type=vi->surf[0]->type;
        vi->color=vi->surf[0]->color;
        s_color[0]=vi->surf[0]->color[0];
        s_color[1]=vi->surf[0]->color[1];
        s_color[2]=vi->surf[0]->color[2];
        s_color[3]=vi->surf[0]->color[3];
//        s_color[3]=1.0;
        venttype=-99;
        if(nn<nvents){
          float s2_color[4];

          s2_color[0]=-1.0;
          s2_color[1]=-1.0;
          s2_color[2]=-1.0;
          s2_color[3]=1.0;


          fgets(buffer,255,stream);
          sscanf(buffer,"%i %i %i %i %i %i %i %i %f %f %f %f",
               &iv1,&iv2,&jv1,&jv2,&kv1,&kv2,
               &ventindex,&venttype,
               s2_color,s2_color+1,s2_color+2,s2_color+3);
          if(s2_color[0]>=0.0&&s2_color[1]>=0.0&&s2_color[2]>=0.0){
            s_color[0]=s2_color[0];
            s_color[1]=s2_color[1];
            s_color[2]=s2_color[2];
            if(s2_color[3]<0.99){
              vi->transparent=1;
              s_color[3]=s2_color[3];
            }
            vi->useventcolor=1;
          }
		      if(ventindex<0)vi->hideboundary=1;
          if(venttype!=-99){
            vi->type=venttype;
          }
          //vi->colorindex=ventindex;
          if(ventindex!=-99&&ventindex!=99){
            if(ventindex<0)ventindex=-ventindex;
            if(ventindex>nrgb2-1)ventindex=nrgb2-1;
            s_color[0]=rgb[nrgb+ventindex][0];
            s_color[1]=rgb[nrgb+ventindex][1];
            s_color[2]=rgb[nrgb+ventindex][2];
            s_color[3]=1.0;
            vi->colorindex=ventindex;
            vi->usecolorindex=1;
            vi->useventcolor=1;
            updateindexcolors=1;
          }
          else{
          }
          vi->color = getcolorptr(s_color);
        }
        else{
          iv1=0; iv2 = meshi->ibar;
          jv1=0; jv2 = meshi->jbar;
          kv1=0; kv2 = meshi->kbar;
          ventindex=-99;
          vi->dir=nn-nvents;
          if(vi->dir>5)vi->dir-=6;
          vi->dir2=0;
          switch (nn-nvents){
          case DOWN_Y:
          case DOWN_Y+6:
            jv2=jv1;
            if(nn>=nvents+6)vi->dir=UP_Y;
            vi->dir2=2;
            break;
          case UP_X:
          case UP_X+6:
            iv1=iv2;
            if(nn>=nvents+6)vi->dir=DOWN_X;
            vi->dir2=1;
            break;
          case UP_Y:
          case UP_Y+6:
            jv1=jv2;
            if(nn>=nvents+6)vi->dir=DOWN_Y;
            vi->dir2=2;
            break;
          case DOWN_X:
          case DOWN_X+6:
            iv2=iv1;
            if(nn>=nvents+6)vi->dir=UP_X;
            vi->dir2=1;
            break;
          case DOWN_Z:
          case DOWN_Z+6:
            kv2=kv1;
            if(nn>=nvents+6)vi->dir=UP_Z;
            vi->dir2=3;
            break;
          case UP_Z:
          case UP_Z+6:
            kv1=kv2;
            if(nn>=nvents+6)vi->dir=DOWN_Z;
            vi->dir2=3;
            break;
          default:
            ASSERT(FFALSE);
            break;
          }
        }
        if(vi->transparent==1)nvent_transparent++;
        vi->linewidth=&ventlinewidth;
        vi->showhide=NULL;
        vi->showtime=NULL;
        vi->showtimelist=NULL;
        vi->xvent1 = xplttemp[iv1];
        vi->xvent2 = xplttemp[iv2]; 
        vi->yvent1 = yplttemp[jv1]; 
        vi->yvent2 = yplttemp[jv2]; 
        vi->zvent1 = zplttemp[kv1]; 
        vi->zvent2 = zplttemp[kv2];
        vi->imin = iv1;
        vi->imax = iv2;
        vi->jmin = jv1;
        vi->jmax = jv2;
        vi->kmin = kv1;
        vi->kmax = kv2;
        if(nn>=nvents&&nn<nvents+6){
          vi->color=foregroundcolor;
        }
        ASSERT(vi->color!=NULL);
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ PART ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"PART") == 1 || match(buffer,"EVAC")==1
      ||match(buffer,"PRT5")==1||match(buffer,"EVA5")==1
      ){
      unsigned int lenkey;

      nn_part++;

      parti = partinfo + ipart;

      parti->version=0;
      if(match(buffer,"PRT5")==1||match(buffer,"EVA5")==1){
        parti->version=1;
      }
      lenkey=4;
      parti->evac=0;
      if(match(buffer,"EVAC")==1
        ||match(buffer,"EVA5")==1
        ){
        parti->evac=1;
        nevac++;
      }
      len=strlen(buffer);
      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(len>lenkey+1){
        buffer3=buffer+lenkey;
        if(parti->evac==1){
          float zoffset=0.0;

          sscanf(buffer3,"%i %f",&blocknumber,&zoffset);
          parti->zoffset=zoffset;
        }
        else{
          sscanf(buffer3,"%i",&blocknumber);
        }
        blocknumber--;
      }

      parti->blocknumber=blocknumber;
      parti->seq_id=nn_part;
      parti->autoload=0;
      if(fgets(buffer,255,stream)==NULL){
        npartinfo--;
        BREAK;
      }

      bufferptr=trim_string(buffer);
      len=strlen(bufferptr);
      parti->reg_file=NULL;
      if(NewMemory((void **)&parti->reg_file,(unsigned int)(len+1))==0)return 2;
      STRCPY(parti->reg_file,bufferptr);

      parti->size_file=NULL;
      if(NewMemory((void **)&parti->size_file,(unsigned int)(len+1+3))==0)return 2;
      STRCPY(parti->size_file,bufferptr);
      STRCAT(parti->size_file,".sz");

      parti->comp_file=NULL;
      if(NewMemory((void **)&parti->comp_file,(unsigned int)(len+1+4))==0)return 2;
      STRCPY(parti->comp_file,bufferptr);
      STRCAT(parti->comp_file,".svz");

      if(STAT(parti->comp_file,&statbuffer)==0){
        parti->compression_type=1;
        parti->file=parti->comp_file;
      }
      else{
        parti->compression_type=0;
        if(STAT(parti->reg_file,&statbuffer)==0){
          parti->file=parti->reg_file;
        }
        else{
          FREEMEMORY(parti->reg_file);
          FREEMEMORY(parti->comp_file);
          FREEMEMORY(parti->size_file);
          parti->file=NULL;
        }
      }
      parti->compression_type=0;
      parti->sort_tags_loaded=0;
      parti->loaded=0;
      parti->display=0;
      parti->times=NULL; 
      parti->xpart=NULL;  
      parti->ypart=NULL;  
      parti->zpart=NULL;  
      parti->xpartb=NULL; 
      parti->ypartb=NULL; 
      parti->zpartb=NULL; 
      parti->xparts=NULL; 
      parti->yparts=NULL; 
      parti->zparts=NULL; 
      parti->tpart=NULL;  
      parti->itpart=NULL; 
      parti->isprink=NULL;
      parti->sframe=NULL; 
      parti->bframe=NULL;
      parti->sprframe=NULL;
      parti->timeslist=NULL;
      parti->particle_type=0;
      parti->droplet_type=0;

      parti->data5=NULL;
      parti->partclassptr=NULL;

      if(parti->version==1){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&parti->nclasses);
        if(parti->nclasses>0){
//          createnulllabel(&parti->label);
          if(parti->file!=NULL)NewMemory((void **)&parti->partclassptr,parti->nclasses*sizeof(part5class *));
          for(i=0;i<parti->nclasses;i++){
            int iclass;
            int ic,iii;

            fgets(buffer,255,stream);
            if(parti->file==NULL)continue;
            sscanf(buffer,"%i",&iclass);
            if(iclass<1)iclass=1;
            if(iclass>npartclassinfo)iclass=npartclassinfo;
            ic=0;
            for(iii=0;iii<npartclassinfo;iii++){
              part5class *pci;

              pci = partclassinfo + iii;
              if(parti->evac==1&&pci->kind!=HUMANS)continue;
              if(parti->evac==0&&pci->kind!=PARTICLES)continue;
              if(iclass-1==ic){
                parti->partclassptr[i]=pci;
                break;
              }
              ic++;
            }
          }
        }
        // if no classes were specifed for the prt5 entry then assign it the default class
        if(parti->file!=NULL&&parti->nclasses==0){
          NewMemory((void **)&parti->partclassptr,sizeof(part5class *));
            parti->partclassptr[i]=partclassinfo + parti->nclasses;
        }
        if(parti->file==NULL||STAT(parti->file,&statbuffer)!=0){
          npartinfo--;
        }
        else{
          ipart++;
        }
      }
      else{
        if(STAT(buffer,&statbuffer)==0){
          if( readlabels(&parti->label,stream)==2 )return 2;
          ipart++;
        }
        else{
          if(readlabels(&parti->label,stream)==2)return 2;
          npartinfo--;
        }
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TARG ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(
      match(buffer,"TARG") == 1||
      match(buffer,"FTARG") == 1
      ){
      targinfo[itarg].type=1;
      if(match(buffer,"FTARG")==1)targinfo[itarg].type=2;

      if(fgets(buffer,255,stream)==NULL){
        ntarginfo--;
        BREAK;
      }
      len=strlen(buffer);
      buffer[len-1]='\0';
      trim(buffer);
      len=strlen(buffer);
      targinfo[itarg].loaded=0;
      targinfo[itarg].display=0;
      if(NewMemory((void **)&targinfo[itarg].file,(unsigned int)(len+1))==0)return 2;
      STRCPY(targinfo[itarg].file,buffer);
      if(targfilename!=NULL){
        FREEMEMORY(targfilename);
        if(NewMemory((void **)&targfilename,(unsigned int)(len+1))==0)return 2;
        STRCPY(targfilename,buffer);
      }
      if(STAT(buffer,&statbuffer)==0){
        itarg++;
      }
      else{
        ntarginfo--;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SYST ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"SYST") == 1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      len=strlen(buffer);
      buffer[len-1]='\0';
      STRCPY(LESsystem,buffer);
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ ENDIAN ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"ENDIAN") == 1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      len=strlen(buffer);
      buffer[len-1]='\0';
      strncpy(LESendian,buffer,1);
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ ENDF ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    /* ENDF superscedes ENDIAN */
    if(match(buffer,"ENDF") == 1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      bufferptr=trim_string(buffer);
      len=strlen(bufferptr);
      NewMemory((void **)&endianfilename,(unsigned int)(len+1));
      strcpy(endianfilename,bufferptr);
      ENDIANfile = fopen(endianfilename,"rb");
      if(ENDIANfile!=NULL){
        endian_native = getendian();
        FSEEK(ENDIANfile,4,SEEK_SET);
        fread(&endian_data,4,1,ENDIANfile);
        fclose(ENDIANfile);
        endian_smv=endian_native;
        if(endian_data!=1)endian_smv=1-endian_native;
        setendian=1;
      }
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ CHID +++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"CHID") == 1){
      if(fgets(buffer,255,stream)==NULL){
        BREAK;
      }
      bufferptr=trim_string(buffer);
      len=strlen(bufferptr);
      FREEMEMORY(chidfilebase);
      NewMemory((void **)&chidfilebase,(unsigned int)(len+1));
      STRCPY(chidfilebase,bufferptr);

      if(chidfilebase!=NULL){
        NewMemory((void **)&hrr_csvfilename,(unsigned int)(strlen(chidfilebase)+8+1));
        STRCPY(hrr_csvfilename,chidfilebase);
        STRCAT(hrr_csvfilename,"_hrr.csv");
        if(STAT(hrr_csvfilename,&statbuffer)!=0){
          FREEMEMORY(hrr_csvfilename);
        }
      }
      continue;
    }

  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ SLCF ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if( (match(buffer,"SLCF") == 1)||
        (match(buffer,"SLCC") == 1)||
        (match(buffer,"SLFL") == 1)||
        (match(buffer,"SLCT") == 1)
      ){
      int terrain=0, cellcenter=0, fire_line=0;
      float above_ground_level=0.0;
      slicedata *sd;
      char *slicelabelptr, slicelabel[256];
      int has_reg, has_comp;
      int i1=-1, i2=-1, j1=-1, j2=-1, k1=-1, k2=-1;
      char *sliceparms;

      sliceparms=strchr(buffer,'&');
      if(1==0&&sliceparms!=NULL){
        sliceparms++;
        sliceparms[-1]=0;
        sscanf(sliceparms,"%i %i %i %i %i %i",&i1,&i2,&j1,&j2,&k1,&k2);
      }

      nn_slice++;
      slicelabelptr=strchr(buffer,'%');
      if(slicelabelptr!=NULL){
        *slicelabelptr=0;
        slicelabelptr++;
        trim(slicelabelptr);
        slicelabelptr=trim_front(slicelabelptr);
        strcpy(slicelabel,slicelabelptr);
        slicelabelptr=slicelabel;
      }
      if(match(buffer,"SLCT") == 1){
        terrain=1;
      }
      if(match(buffer,"SLFL") == 1){
        terrain=1;
        fire_line=1;
      }
      if(match(buffer,"SLCC") == 1){
        cellcenter_slice_active=1;
        cellcenter=1;
      }
      trim(buffer);
      len=strlen(buffer);
      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(len>5){
        buffer3=buffer+4;
        sscanf(buffer3,"%i %f",&blocknumber,&above_ground_level);
        blocknumber--;
      }
      if(fgets(buffer,255,stream)==NULL){
        nsliceinfo--;
        BREAK;
      }

      bufferptr=trim_string(buffer);
      len=strlen(bufferptr);
      
      sd = sliceinfo+islice;
      sd->slicetype=SLICE_NODE;
      if(terrain==1){
        sd->slicetype=SLICE_TERRAIN;
      }
      if(fire_line==1)sd->slicetype=SLICE_FIRELINE;
      if(cellcenter==1){
        sd->slicetype=SLICE_CENTER;
      }

      if(nslicefiles>100&&(islicecount%100==1||nslicefiles==islicecount)){
        printf("     examining %i'th slice file\n",islicecount);
      }
      islicecount++;
      strcpy(buffer2,bufferptr);
      strcat(buffer2,".svz");
      has_reg=0;
      has_comp=0;
      if(file_exists(buffer2)==1)has_comp=1;
      if(has_comp==0&&file_exists(bufferptr)==1)has_reg=1;
      if(has_reg==0&&has_comp==0){
        if(fgets(buffer,255,stream)==NULL){
          nsliceinfo--;
          nslicefiles--;
          BREAK;
        }
        if(fgets(buffer,255,stream)==NULL){
          nsliceinfo--;
          nslicefiles--;
          BREAK;
        }
        if(fgets(buffer,255,stream)==NULL){
          nsliceinfo--;
          nslicefiles--;
          BREAK;
        }
        nsliceinfo--;
        nslicefiles--;
        continue;
      }

      NewMemory((void **)&sd->reg_file,(unsigned int)(len+1));
      STRCPY(sd->reg_file,bufferptr);

      NewMemory((void **)&sd->comp_file,(unsigned int)(len+4+1));
      STRCPY(sd->comp_file,buffer2);

      sd->compression_type=0;
      if(has_comp==1){
        sd->compression_type=1;
        sd->file=sd->comp_file;
      }
      if(sd->compression_type==0){
        sd->file=sd->reg_file;
      }

      if(sd->slicetype==SLICE_TERRAIN){
        if(readlabels_terrain(&sd->label,stream)==2)return 2;
      }
      else if(sd->slicetype==SLICE_CENTER){
        if(readlabels_cellcenter(&sd->label,stream)==2)return 2;
      }
      else{
        if(readlabels(&sd->label,stream)==2)return 2;
      }
      

      {
        char volfile[1024];

        strcpy(volfile,bufferptr);
        strcat(volfile,".svv");
        sd->vol_file=NULL;
        if(file_exists(volfile)==1){
          NewMemory((void **)&sd->vol_file,(unsigned int)(len+4+1));
          STRCPY(sd->vol_file,volfile);
          have_volcompressed=1;
        }
      }

      NewMemory((void **)&sd->size_file,(unsigned int)(len+3+1));
      STRCPY(sd->size_file,bufferptr);
      STRCAT(sd->size_file,".sz");

      sd->slicelabel=NULL;
      if(slicelabelptr!=NULL){
        int lenslicelabel;

        lenslicelabel=strlen(slicelabel)+1;
        NewMemory((void **)&sd->slicelabel,lenslicelabel);
        strcpy(sd->slicelabel,slicelabel);
      }
      sd->is1=i1;
      sd->is2=i2;
      sd->js1=j1;
      sd->js2=j2;
      sd->ks1=k1;
      sd->ks2=k2;
      sd->is_fed=0;
      sd->above_ground_level=above_ground_level;
      sd->seq_id=nn_slice;
      sd->autoload=0;
      sd->display=0;
      sd->loaded=0;
      sd->qslicedata=NULL;
      sd->compindex=NULL;
      sd->slicecomplevel=NULL;
      sd->qslicedata_compressed=NULL;
      sd->volslice=0;
      sd->times=NULL;
      sd->slicelevel=NULL;
      sd->slicepoint=NULL;
      sd->slicedata=NULL;
      sd->timeslist=NULL;
      sd->blocknumber=blocknumber;
      sd->vloaded=0;
      sd->reload=0;
      sd->nline_contours=0;
      sd->line_contours=NULL;
      sd->contour_areas=NULL;
      sd->ncontour_areas=0;
      sd->menu_show=1;
      sd->constant_color=NULL;
      {
        mesh *meshi;

        meshi = meshinfo + blocknumber;
        sd->mesh_type=meshi->mesh_type;
      }
      islice++;
      continue;
    }
  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ BNDF ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */
    if(match(buffer,"BNDF") == 1||match(buffer,"BNDC") == 1||match(buffer,"BNDE") == 1){
      patchdata *patchi;

      nn_patch++;

      trim(buffer);
      len=strlen(buffer);

      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      version=0;
      if(len>5){
        buffer3=buffer+4;
        sscanf(buffer3,"%i %i",&blocknumber,&version);
        blocknumber--;
      }
      patchi = patchinfo + ipatch;

      patchi->version=version;
  
      patchi->filetype=0;
      if(match(buffer,"BNDC") == 1){
        patchi->filetype=1;
        cellcenter_bound_active=1;
      }
      if(match(buffer,"BNDE") == 1){
        patchi->filetype=2;
      }

      if(fgets(buffer,255,stream)==NULL){
        npatchinfo--;
        BREAK;
      }

      bufferptr=trim_string(buffer);
      len=strlen(bufferptr);
      NewMemory((void **)&patchi->reg_file,(unsigned int)(len+1));
      STRCPY(patchi->reg_file,bufferptr);

      NewMemory((void **)&patchi->comp_file,(unsigned int)(len+4+1));
      STRCPY(patchi->comp_file,bufferptr);
      STRCAT(patchi->comp_file,".svz");

      NewMemory((void **)&patchi->size_file,(unsigned int)(len+4+1));
      STRCPY(patchi->size_file,bufferptr);
//      STRCAT(patchi->size_file,".szz"); when we actully use file check both .sz and .szz extensions

      if(STAT(patchi->comp_file,&statbuffer)==0){
        patchi->compression_type=1;
        patchi->file=patchi->comp_file;
      }
      else{
        patchi->compression_type=0;
        patchi->file=patchi->reg_file;
      }

      patchi->geomfile=NULL;
      patchi->geom=NULL;
      if(patchi->filetype==2){
        int igeom;

        if(fgets(buffer,255,stream)==NULL){
          npatchinfo--;
          BREAK;
        }
        bufferptr=trim_string(buffer);
        NewMemory((void **)&patchi->geomfile,strlen(bufferptr)+1);
        strcpy(patchi->geomfile,bufferptr);
        for(igeom=0;igeom<ngeominfo;igeom++){
          geomdata *geomi;

          geomi = geominfo + igeom;
          if(strcmp(geomi->file,patchi->geomfile)==0){
            patchi->geom=geomi;
            break;
          }
        }
      }
      patchi->geom_timeslist=NULL;
      patchi->geom_ivals_dynamic=NULL;
      patchi->geom_ivals_static=NULL;
      patchi->geom_ndynamics=NULL;
      patchi->geom_nstatics=NULL;
      patchi->geom_times=NULL;
      patchi->geom_vals=NULL;
      patchi->geom_ivals=NULL;
      patchi->geom_nvals=0;
      patchi->blocknumber=blocknumber;
      patchi->seq_id=nn_patch;
      patchi->autoload=0;
      patchi->loaded=0;
      patchi->display=0;
      patchi->inuse=0;
      patchi->inuse_getbounds=0;
      patchi->bounds.defined=0;
      patchi->unit_start=unit_start++;
      patchi->setchopmin=0;
      patchi->chopmin=1.0;
      patchi->setchopmax=0;
      patchi->chopmax=0.0;
      meshinfo[blocknumber].patchfilenum=-1;
      if(STAT(patchi->file,&statbuffer)==0){
        if(patchi->filetype==1){
          if(readlabels_cellcenter(&patchi->label,stream)==2)return 2;
        }
        else if(patchi->filetype==0){
          if(readlabels(&patchi->label,stream)==2)return 2;
        }
        else if(patchi->filetype==2){
          if(readlabels(&patchi->label,stream)==2)return 2;
        }
        NewMemory((void **)&patchi->histogram,sizeof(histogramdata));
        init_histogram(patchi->histogram);
        ipatch++;
      }
      else{
        if(trainer_mode==0){
          char message[1024];

          strcpy(message,_("the file, "));
          strcat(message,buffer);
          strcat(message,_(" does not exist"));
          warning_message(message);
        }
        if(readlabels(&patchi->label,stream)==2)return 2;
        npatchinfo--;
      }
      continue;
    }

    /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ ISOF ++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

    if(match(buffer,"ISOF") == 1||match(buffer,"TISOF")==1||match(buffer,"ISOG") == 1){
      isodata *isoi;
      int get_isolevels;
      int dataflag=0,geomflag=0;
      char tbuffer[255], *tbufferptr;

      isoi = isoinfo + iiso;
      nn_iso++;
      if(match(buffer,"TISOF")==1)dataflag=1;
      if(match(buffer,"ISOG")==1)geomflag=1;
      trim(buffer);
      len=strlen(buffer);

      if(nmeshes>1){
        blocknumber=ioffset-1;
      }
      else{
        blocknumber=0;
      }
      if(len>5&&dataflag==0){
        buffer3=buffer+4;
        sscanf(buffer3,"%i",&blocknumber);
        blocknumber--;
      }
      if(len>6&&dataflag==1){
        buffer3=buffer+5;
        sscanf(buffer3,"%i",&blocknumber);
        blocknumber--;
      }
      if(fgets(buffer,255,stream)==NULL){
        nisoinfo--;
        BREAK;
      }

      isoi->tfile=NULL;
      isoi->seq_id=nn_iso;
      isoi->autoload=0;
      isoi->blocknumber=blocknumber;
      isoi->loaded=0;
      isoi->display=0;
      isoi->dataflag=dataflag;
      isoi->geomflag=geomflag;
      isoi->nlevels=0;
      isoi->levels=NULL;
      isoi->is_fed=0;

      isoi->normaltable=NULL;
      isoi->color_label.longlabel=NULL;
      isoi->color_label.shortlabel=NULL;
      isoi->color_label.unit=NULL;
      isoi->geominfo=NULL;
      NewMemory((void **)&isoi->geominfo,sizeof(geomdata));
      init_geom(isoi->geominfo);

      bufferptr=trim_string(buffer);

      len=strlen(bufferptr);

      NewMemory((void **)&isoi->reg_file,(unsigned int)(len+1));
      STRCPY(isoi->reg_file,bufferptr);

      NewMemory((void **)&isoi->size_file,(unsigned int)(len+3+1));
      STRCPY(isoi->size_file,bufferptr);
      STRCAT(isoi->size_file,".sz");

      if(dataflag==1&&geomflag==1){
        if(fgets(tbuffer,255,stream)==NULL){
          nisoinfo--;
          BREAK;
        }
        trim(tbuffer);
        tbufferptr=trim_front(tbuffer);
        NewMemory((void **)&isoi->tfile,strlen(tbufferptr)+1);
        strcpy(isoi->tfile,tbufferptr);
      }

      if(STAT(isoi->reg_file,&statbuffer2)==0){
        get_isolevels=1;
        isoi->file=isoi->reg_file;
        if(readlabels(&isoi->surface_label,stream)==2)return 2;
        if(geomflag==1){
          int ntimes_local;
          geomdata *geomi;
          float **colorlevels,*levels;

          isoi->geominfo->file=isoi->file;
          geomi = isoi->geominfo;
          geomi->file=isoi->file;
          read_geom_header(geomi,&ntimes_local);
          isoi->nlevels=geomi->nfloat_vals;
          if(isoi->nlevels>0){
            NewMemory((void **)&levels,isoi->nlevels*sizeof(float));
            NewMemory((void **)&colorlevels,isoi->nlevels*sizeof(float *));
            for(i=0;i<isoi->nlevels;i++){
              colorlevels[i]=NULL;
              levels[i]=geomi->float_vals[i];
            }
            isoi->levels=levels;
            isoi->colorlevels=colorlevels;
          }
        }
        else{
          getisolevels(isoi->file,dataflag,&isoi->levels,&isoi->colorlevels,&isoi->nlevels);
        }
        if(dataflag==1){
          if(readlabels(&isoi->color_label,stream)==2)return 2;
        }
        iiso++;
      }
      else{
        get_isolevels=0;
        if(trainer_mode==0){
          char message[1024];

          strcpy(message,_("the file, "));
          strcat(message,buffer);
          strcat(message,_(" does not exist"));
          warning_message(message);
        }

        if(readlabels(&isoi->surface_label,stream)==2)return 2;
        if(dataflag==1){
          if(readlabels(&isoi->color_label,stream)==2)return 2;
        }
        nisoinfo--;
      }
      if(get_isolevels==1){
        int len_clevels;
        char clevels[1024];

        array2string(isoi->levels,isoi->nlevels,clevels);
        len_clevels = strlen(clevels);
        if(len_clevels>0){
          int len_long;
          char *long_label, *unit_label;

          long_label = isoi->surface_label.longlabel;
          unit_label = isoi->surface_label.unit;
          len_long = strlen(long_label)+strlen(unit_label)+len_clevels+3+1;
          if(dataflag==1)len_long+=(strlen(isoi->color_label.longlabel)+15+1);
          ResizeMemory((void **)&long_label,(unsigned int)len_long);
          isoi->surface_label.longlabel=long_label;
          strcat(long_label,": ");
          strcat(long_label,clevels);
          strcat(long_label," ");
          strcat(long_label,unit_label);
          if(dataflag==1){
            strcat(long_label," (Colored by: ");
            strcat(long_label,isoi->color_label.longlabel);
            strcat(long_label,")");
          }
        }
      }
      continue;
    }

  }

/* 
   ************************************************************************
   ************************ end of pass 4 ********************************* 
   ************************************************************************
 */

  if(autoterrain==1){
    float zbarmin;

    zbarmin=meshinfo->xyz_bar0[ZZZ];
    for(i=1;i<nmeshes;i++){
      mesh *meshi;

      meshi = meshinfo + i;
      if(meshi->xyz_bar0[ZZZ]<zbarmin)zbarmin=meshi->xyz_bar0[ZZZ];
    }
    nobst=0;
    iobst=0;
    for(i=0;i<nmeshes;i++){
      mesh *meshi;
      int ibar, jbar;

      meshi = meshinfo + i;
      ibar = meshi->ibar;
      jbar = meshi->jbar;
      if(ibar>0&&jbar>0){
        float *zcell;

        NewMemory((void **)&meshi->zcell,ibar*jbar*sizeof(float));
        zcell = meshi->zcell;
        for(j=0;j<ibar*jbar;j++){
          zcell[j]=zbarmin; // assume initially that zcell (terrain height) is at base of the domain
        }
      }
    }
  }

  for(i=0;i<nmeshes;i++){
    mesh *meshi;
    int nlist;

    meshi=meshinfo+i;

    nlist=meshi->nsmoothblockages_list+2; // add an entry for t=0.0
    NewMemory((void **)&meshi->smoothblockages_list,nlist*sizeof(smoothblockage));

    meshi->nsmoothblockages_list++;

    for(j=0;j<nlist;j++){
      smoothblockage *sb;

      sb=meshi->smoothblockages_list+j;
      sb->smoothblockagecolors=NULL;
      sb->smoothblockagesurfaces=NULL;
      sb->nsmoothblockagecolors=0;
    }
    meshi->nsmoothblockages_list=1;
    meshi->smoothblockages_list[0].time=-1.0;
    meshi->nsmoothblockages_list++;
  }

  /* 
   ************************************************************************
   ************************ start of pass 5 ********************************* 
   ************************************************************************
 */

  rewind(stream1);
  if(stream2!=NULL)rewind(stream2);
  stream=stream1;
  printf("%s",_("   pass 4 "));
  printf("%s",_("completed"));
  printf("\n");
  if(do_pass4==1||autoterrain==1){
    printf("%s",_("   pass 5 started"));
    printf("\n");
  }

  while((autoterrain==1||do_pass4==1)){
    if(feof(stream)!=0){
      BREAK;
    }

    if(fgets(buffer,255,stream)==NULL){
      BREAK;
    }
    if(strncmp(buffer," ",1)==0||buffer[0]==0)continue;

    if(match(buffer,"MINMAXBNDF") == 1){
      char *file_ptr, file2_local[1024];
      float valmin, valmax;
      float percentile_min, percentile_max;

      fgets(buffer,255,stream);
      strcpy(file2_local,buffer);
      file_ptr = file2_local;
      trim(file2_local);
      file_ptr = trim_front(file2_local);

      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f %f",&valmin,&valmax,&percentile_min,&percentile_max);

      for(i=0;i<npatchinfo;i++){
        patchdata *patchi;

        patchi = patchinfo + i;
        if(strcmp(file_ptr,patchi->file)==0){
          patchi->diff_valmin=percentile_min;
          patchi->diff_valmax=percentile_max;
          break;
        }
      }
      continue;
    }
    if(match(buffer,"MINMAXSLCF") == 1){
      char *file_ptr, file2_local[1024];
      float valmin, valmax;
      float percentile_min, percentile_max;

      fgets(buffer,255,stream);
      strcpy(file2_local,buffer);
      file_ptr = file2_local;
      trim(file2_local);
      file_ptr = trim_front(file2_local);

      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f %f",&valmin,&valmax,&percentile_min,&percentile_max);

      for(i=0;i<nsliceinfo;i++){
        slicedata *slicei;

        slicei = sliceinfo + i;
        if(strcmp(file_ptr,slicei->file)==0){
          slicei->diff_valmin=percentile_min;
          slicei->diff_valmax=percentile_max;
          break;
        }
      }
      continue;
    }
    if(match(buffer,"OBST") == 1&&autoterrain==1){
      mesh *meshi;
      int nxcell;
      int n_blocks;
      int iblock;

      nobst++;
      iobst++;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n_blocks);


      if(n_blocks<=0)n_blocks=0;
      if(n_blocks==0)continue;

      meshi=meshinfo+iobst-1;

      for(nn=0;nn<n_blocks;nn++){
        fgets(buffer,255,stream);
      }
      nxcell = meshi->ibar;
      nn=-1;
      for(iblock=0;iblock<n_blocks;iblock++){
        int ijk2[5],kmax;
        int ii, jj;
        float zval;

        if(meshi->is_block_terrain!=NULL&&meshi->is_block_terrain[iblock]==0){
          fgets(buffer,255,stream);
          continue;
        }
        nn++;

        fgets(buffer,255,stream);
        sscanf(buffer,"%i %i %i %i %i %i",ijk2,ijk2+1,ijk2+2,ijk2+3,ijk2+4,&kmax);
        zval = meshi->zplt[kmax];
        for(ii=ijk2[0];ii<ijk2[1];ii++){
          for(jj=ijk2[2];jj<ijk2[3];jj++){
            int ij;
            float zcell;

            ij = IJCELL2(ii,jj);
            zcell = meshi->zcell[ij];
            if(zval>zcell){
              meshi->zcell[ij]=zval;
            }
          }
        }
      }
      FREEMEMORY(meshi->is_block_terrain);
      continue;
    }
  }

  if(do_pass4==1||autoterrain==1){
    printf("%s",_("   pass 5 "));
    printf("%s",_("completed"));
    printf("\n");
  }

  printf("%s processing completed\n",file);
  printf("\n");
  printf("%s",_("beginning wrap up "));
  printf("\n");
#ifdef _DEBUG
  PrintMemoryInfo;
#endif

/* 
   ************************************************************************
   ************************ wrap up *************************************** 
   ************************************************************************
 */

  update_isocolors();
  CheckMemory;

  //remove_dup_blockages(); //xxx remove_dup
  initcullgeom(cullgeom);
  init_evac_prop();

  update_inilist();

  if(meshinfo!=NULL&&meshinfo->jbar==1)force_isometric=1;

// update csv data

  if(hrr_csvfilename!=NULL)readhrr(LOAD, &errorcode);
  read_device_data(NULL,CSV_FDS,UNLOAD);
  read_device_data(NULL,CSV_EXP,UNLOAD);
  for(i=0;i<ncsvinfo;i++){
    csvdata *csvi;

    csvi = csvinfo + i;
    if(csvi->type==CSVTYPE_DEVC)read_device_data(csvi->file,CSV_FDS,LOAD);
    if(csvi->type==CSVTYPE_EXT)read_device_data(csvi->file,CSV_EXP,LOAD);
  }
  setup_device_data();

  init_multi_threading();

  init_part5prop();

  if(noutlineinfo>0){
    highlight_flag=2;
  }
  else{
    highlight_flag=1;
  }
  if(dummyvents==1){
    visFloor=0;
    visCeiling=0;
    visWalls=0;
  }
  initcadcolors();

  // update loaded lists

  FREEMEMORY(slice_loaded_list);
  if(nsliceinfo>0){
    NewMemory((void **)&slice_loaded_list,nsliceinfo*sizeof(int));
  }
  FREEMEMORY(patch_loaded_list);
  if(npatchinfo>0){
    NewMemory((void **)&patch_loaded_list,npatchinfo*sizeof(int));
  }
  update_loaded_lists();

  update_mesh_coords();

  /*
    Associate a surface with each block.
  */
  updateusetextures();

  /* compute global bar's and box's */


  for(i=0;i<npartclassinfo;i++){
    part5class *partclassi;

    partclassi = partclassinfo + i;

    if(partclassi->device_name!=NULL){
        float length, azimuth, elevation;
       
        partclassi->diameter/=xyzmaxdiff;
        partclassi->length/=xyzmaxdiff;
        length=partclassi->length;
        azimuth = partclassi->azimuth*PI/180.0;
        elevation = partclassi->elevation*PI/180.0;
        partclassi->dx = cos(azimuth)*cos(elevation)*length/2.0;
        partclassi->dy = sin(azimuth)*cos(elevation)*length/2.0;
        partclassi->dz =              sin(elevation)*length/2.0;
    }
  }

#ifdef pp_SHOOTER
  shooter_xyz[0]=xbar/2.0;
  shooter_xyz[1] = 0.0;
  shooter_xyz[2] = zbar/2.0;
  shooter_dxyz[0]=xbar/4.0;
  shooter_dxyz[1]=0.0;
  shooter_dxyz[2]=0.0;
  shooter_nparts=100;
  shooter_velmag=1.0;
  shooter_veldir=0.0;
  shooter_fps=10;
  shooter_vel_type=1;
#endif


  update_plotxyz_all();

  updatevslices();
  
  active_smokesensors=0;
  for(i=0;i<ndeviceinfo;i++){
    devicedata *devicei;
    char *label;

    devicei = deviceinfo + i;
    devicei->device_mesh=getmesh(devicei->xyz);
    label = devicei->object->label;
    if(strcmp(label,"smokesensor")==0){
      active_smokesensors=1;
    }
    if(devicei->plane_surface!=NULL){
      init_device_plane(devicei);
    }
  }


  makeiblank();
  makeiblank_carve();
  makeiblank_smoke3d();
  setventdirs();
  update_faces();

  xcenGLOBAL=xbar/2.0;  ycenGLOBAL=ybar/2.0; zcenGLOBAL=zbar/2.0;

  update_endian_info();

  update_bound_info();

  // close .smv file

  fclose(stream1);
  if(stream2!=NULL)fclose(stream2);
  stream=NULL;

  update_selectfaces();
  updateslicetypes();
  updatesliceboundlabels();
  updateisotypes();
  updatepatchtypes();
  if(autoterrain==1){
    for(i=0;i<nmeshes;i++){
      mesh *meshi;
      float *zcell;

      meshi = meshinfo + i;
      zcell = meshi->zcell;

      zterrain_min = zcell[0];
      zterrain_max = zterrain_min;
      for(j=1;j<meshi->ibar*meshi->jbar;j++){
        float zval;

        zval=*zcell++;
        if(zval<zterrain_min)zterrain_min=zval;
        if(zval>zterrain_max)zterrain_max=zval;
      }
    }
  }
  update_terrain(1,vertical_factor);
  update_terrain_colors();
  updatesmoke3dmenulabels();
  updatevslicetypes();
  updatepatchmenulabels();
  updateisomenulabels();
  updatepartmenulabels();
  updatetourmenulabels();
  init_user_ticks();
  clip_I=ibartemp; clip_J=jbartemp; clip_K=kbartemp;

  // define changed_idlist used for blockage editing

  {
    int ntotal=0;

    for(i=0;i<nmeshes;i++){
      mesh *meshi;

      meshi = meshinfo + i;
      ntotal += meshi->nbptrs;
    }
    FREEMEMORY(changed_idlist);

    NewMemory((void **)&changed_idlist,sizeof(int)*(ntotal+1));

    for(i=0;i<ntotal;i++){
      changed_idlist[i]=0;
    }
    nchanged_idlist=ntotal;
  }

  init_volrender();

#ifdef pp_CULL

  // define data structures used to speed up 3d smoke drawing (by culling non-visible smoke planes)

  if(cullactive==1)initcull(cullsmoke);
#endif
  update_mesh_terrain();

  read_all_geom();
  ngeominfoptrs=0;
  GetGeomInfoPtrs(&geominfoptrs,&ngeominfoptrs);
  update_triangles();
  get_faceinfo();

  printf("%s",_("wrap up completed"));
  printf("\n\n");
#ifdef _DEBUG
  PrintMemoryInfo;
#endif

  return 0;
}

/* ------------------ parsedatabase ------------------------ */

void parsedatabase(char *file){
  FILE *stream;
  char buffer[1000],*buffer2=NULL,*buffer3,*slashptr;
  size_t lenbuffer,lenbuffer2;
  size_t sizebuffer2;
  char *surf_id=NULL,*start,*surf_id2;
  char *c;
  int i,j;
  surfdata *surfj;
  char *labeli, *labelj;
  int nexti;
  int nsurfids_shown;

  /* free memory called before */

  for(i=0;i<nsurfids;i++){
    surf_id=surfids[i].label;
    FREEMEMORY(surf_id);
  }
  FREEMEMORY(surfids);
  nsurfids=0;


  if( file==NULL||strlen(file)==0||(stream=fopen(file,"r"))==NULL){
    NewMemory((void **)&surfids,(nsurfids+1)*sizeof(surfid));
    surf_id=NULL;
    NewMemory((void **)&surf_id,6);
    strcpy(surf_id,"INERT");
    surfids[0].label=surf_id;
    surfids[0].location=0;
    surfids[0].show=1;
    nsurfids=1;
  }

  else{
    sizebuffer2=1001;
    NewMemory((void **)&buffer2,sizebuffer2);

  /* find out how many surfs are in database file so memory can be allocated */
  
    while(!feof(stream)){
      if(fgets(buffer,1000,stream)==NULL)break;
      if(STRSTR(buffer,"&SURF")==NULL)continue;


      slashptr=strstr(buffer,"/");
      if(slashptr!=NULL)strcpy(buffer2,buffer);
      buffer3=buffer;
      while(slashptr!=NULL){
        fgets(buffer,1000,stream);
        lenbuffer=strlen(buffer);
        lenbuffer2=strlen(buffer2);
        if(lenbuffer2+lenbuffer+2>sizebuffer2){
          sizebuffer2 = lenbuffer2+lenbuffer+2+1000;
          ResizeMemory((void **)&buffer2,(unsigned int)sizebuffer2);
        }
        strcat(buffer2,buffer);
        slashptr=strstr(buffer,"/");
        buffer3=buffer2;
      }
      start=STRSTR(buffer3,"ID");
      if(start!=NULL)nsurfids++;
    }

  /* allocate memory */

    NewMemory((void **)&surfids,(nsurfids+1)*sizeof(surfid));
    surf_id=NULL;
    NewMemory((void **)&surf_id,6);
    strcpy(surf_id,"INERT");
    surfids[0].label=surf_id;
    surfids[0].location=0;
    surfids[0].show=1;


  /* now look for IDs and copy them into an array */

    rewind(stream);
    nsurfids=1;
    while(!feof(stream)){
      if(fgets(buffer,1000,stream)==NULL)break;
      if(STRSTR(buffer,"&SURF")==NULL)continue;
   

      slashptr=strstr(buffer,"/");
      if(slashptr!=NULL)strcpy(buffer2,buffer);
      while(slashptr!=NULL){
        fgets(buffer,1000,stream);
        lenbuffer=strlen(buffer);
        lenbuffer2=strlen(buffer2);
        if(lenbuffer2+lenbuffer+2>sizebuffer2){
          sizebuffer2 = lenbuffer2+lenbuffer+2+1000;
          ResizeMemory((void **)&buffer2,(unsigned int)sizebuffer2);
        }
        strcat(buffer2,buffer);
        slashptr=strstr(buffer,"/");
        buffer3=buffer2;
      }
      start=STRSTR(buffer3+3,"ID");
      if(start!=NULL)nsurfids++;
      surf_id=NULL;
      surf_id2=NULL;
      for(c=start;c!=NULL&&*c!='\0';c++){
        if(surf_id==NULL&&*c=='\''){
          surf_id=c+1;
          continue;
        }
        if(surf_id!=NULL&&*c=='\''){
          *c='\0';
          NewMemory((void **)&surf_id2,strlen(surf_id)+1);
          strcpy(surf_id2,surf_id);
          surfids[nsurfids-1].label=surf_id2;
          surfids[nsurfids-1].location=1;
          surfids[nsurfids-1].show=1;
          break;
        }
      }

    }
  }

  /* identify duplicate surfaces */
  /*** debug: make sure ->show is defined for all cases ***/

  nsurfids_shown=0;
  for(i=0;i<nsurfids;i++){
    labeli = surfids[i].label;
    nexti = 0;
    for(j=0;j<nsurfinfo;j++){
      surfj = surfinfo + j;
      labelj = surfj->surfacelabel;
      if(strcmp(labeli,labelj)==0){
        nexti = 1;
        break;
      }
    }
    if(nexti==1){
      surfids[i].show=0;
      continue;
    }
    for(j=0;j<i;j++){
      labelj = surfids[j].label;
      if(strcmp(labeli,labelj)==0){
        nexti = 1;
        break;
      }
    }
    if(nexti==1){
      surfids[i].show=0;
      continue;
    }
    nsurfids_shown++;

  }

  /* add surfaces found in database to those surfaces defined in previous SURF lines */

  if(nsurfids_shown>0){
    if(nsurfinfo==0){
      FREEMEMORY(surfinfo);
      FREEMEMORY(textureinfo);
      NewMemory((void **)&surfinfo,nsurfids_shown*sizeof(surfdata));
      NewMemory((void **)&textureinfo,nsurfids_shown*sizeof(surfdata));
    }
    if(nsurfinfo>0){
      if(surfinfo==NULL){
        NewMemory((void **)&surfinfo,(nsurfids_shown+nsurfinfo)*sizeof(surfdata));
      }
      else{
        ResizeMemory((void **)&surfinfo,(nsurfids_shown+nsurfinfo)*sizeof(surfdata));
      }
      if(textureinfo==NULL){
        NewMemory((void **)&textureinfo,(nsurfids_shown+nsurfinfo)*sizeof(surfdata));
      }
      else{
        ResizeMemory((void **)&textureinfo,(nsurfids_shown+nsurfinfo)*sizeof(surfdata));
      }
    }
    surfj = surfinfo + nsurfinfo - 1;
    for(j=0;j<nsurfids;j++){
      if(surfids[j].show==0)continue;
      surfj++;
      initsurface(surfj);
      surfj->surfacelabel=surfids[j].label;
    }
    nsurfinfo += nsurfids_shown;
  }
  update_sorted_surfidlist();
}

/* ------------------ surfid_compare ------------------------ */

int surfid_compare( const void *arg1, const void *arg2 ){
  surfdata *surfi, *surfj;

  int i, j;
  i=*(int *)arg1;
  j=*(int *)arg2;

  surfi = surfinfo + i;
  surfj = surfinfo + j;

  return(strcmp(surfi->surfacelabel,surfj->surfacelabel));
}

/* ------------------ updated_sorted_surfidlist ------------------------ */

void update_sorted_surfidlist(void){
  int i;

  FREEMEMORY(sorted_surfidlist);
  FREEMEMORY(inv_sorted_surfidlist);
  NewMemory((void **)&sorted_surfidlist,nsurfinfo*sizeof(int));
  NewMemory((void **)&inv_sorted_surfidlist,nsurfinfo*sizeof(int));


  nsorted_surfidlist=nsurfinfo;
  for(i=0;i<nsorted_surfidlist;i++){
    sorted_surfidlist[i]=i;
  }


  qsort( (int *)sorted_surfidlist, (size_t)nsurfinfo, sizeof(int), surfid_compare );
  for(i=0;i<nsorted_surfidlist;i++){
    inv_sorted_surfidlist[sorted_surfidlist[i]]=i;
  }


}


        /* 
        new OBST format:
        i1 i2 j1 j2 k1 k2 colorindex blocktype       : if blocktype!=1&&colorindex!=-3
        i1 i2 j1 j2 k1 k2 colorindex blocktype r g b : if blocktype!=1&&colorindex==-3
        i1 i2 j1 j2 k1 k2 ceiling texture blocktype [wall texture floor texture] : if blocktype==1
        int colorindex, blocktype;
        colorindex: -1 default color
                    -2 invisible
                    -3 use r g b color
                    >=0 color/color2/texture index
        blocktype: 0 regular block non-textured
                   1 regular block textured
                   2 outline
                   3 smoothed block
        r g b           colors used if colorindex==-3
        */

/* ------------------ backup_blockage ------------------------ */

void backup_blockage(blockagedata *bc){
  int i;

  bc->xyzORIG[0]=bc->xmin;
  bc->xyzORIG[1]=bc->xmax;
  bc->xyzORIG[2]=bc->ymin;
  bc->xyzORIG[3]=bc->ymax;
  bc->xyzORIG[4]=bc->zmin;
  bc->xyzORIG[5]=bc->zmax;
  bc->ijkORIG[0]=bc->ijk[0];
  bc->ijkORIG[1]=bc->ijk[1];
  bc->ijkORIG[2]=bc->ijk[2];
  bc->ijkORIG[3]=bc->ijk[3];
  bc->ijkORIG[4]=bc->ijk[4];
  bc->ijkORIG[5]=bc->ijk[5];
  for(i=0;i<6;i++){
    bc->surfORIG[i]=bc->surf[i];
    bc->surf_indexORIG[i]=bc->surf_index[i];
  }
}

/* ------------------ ifsmoothblock ------------------------ */

int ifsmoothblock(void){
  int i,j;
  mesh *meshi;
  blockagedata *bc;

  for(i=0;i<nmeshes;i++){
    meshi = meshinfo + i;
    for(j=0;j<meshi->nbptrs;j++){
      bc = meshi->blockageinfoptrs[j];
      if(bc->type==BLOCK_smooth&&bc->del!=1)return 1;
    }
  }
  return 0;
}
/* ------------------ updateusetextures ------------------------ */

void updateusetextures(void){
  int i,j,k;
  mesh *meshi;
  blockagedata *bc;
  texture *texti;
  ventdata *vi;

  for(i=0;i<ntextures;i++){
    texti=textureinfo + i;
    texti->used=0;
  }
  for(i=0;i<nmeshes;i++){
    meshi=meshinfo + i;
    if(textureinfo!=NULL){
      for(j=0;j<meshi->nbptrs;j++){
        bc=meshi->blockageinfoptrs[j];
        for(k=0;k<6;k++){
          texti = bc->surf[k]->textureinfo;
          if(texti!=NULL&&texti->loaded==1){
            if(texti->loaded==1&&bc->type==BLOCK_smooth)bc->type=BLOCK_texture;
            if(usetextures==1)texti->display=1;
            texti->used=1;
          }
        }
      }
    }
    for(j=0;j<meshi->nvents+12;j++){
      vi = meshi->ventinfo + j;
      if(vi->surf[0]==NULL){
        if(vent_surfacedefault!=NULL){
          if(j>=meshi->nvents+6){
            vi->surf[0]=exterior_surfacedefault;
          }
          else{
            vi->surf[0]=vent_surfacedefault;
          }
        }
      }
      if(textureinfo!=NULL){
        if(vi->surf[0]!=NULL){
          texti = vi->surf[0]->textureinfo;
          if(texti!=NULL&&texti->loaded==1){
            if(usetextures==1)texti->display=1;
            texti->used=1;
          }
        }
      }
    }
  }
  for(i=0;i<ndevice_texture_list;i++){
    int texture_index;

    texture_index  = device_texture_list_index[i];
    texti=textureinfo + texture_index;
    if(texti!=NULL&&texti->loaded==1){
      if(usetextures==1)texti->display=1;
      texti->used=1;
    }
  }
  ntextures_loaded_used=0;
  for(i=0;i<ntextures;i++){
    texti = textureinfo + i;
    if(texti->loaded==0)continue;
    if(texti->used==0)continue;
    ntextures_loaded_used++;
  }
}

/* ------------------ initsurface ------------------------ */

void initsurface(surfdata *surf){
  surf->iso_level=-1;
  surf->used_by_obst=0;
  surf->used_by_vent=0;
  surf->emis=1.0;
  surf->temp_ignition=TEMP_IGNITION_MAX;
  surf->surfacelabel=NULL;
  surf->texturefile=NULL;
  surf->textureinfo=NULL;
  surf->color=block_ambient2;
  surf->t_width=1.0;
  surf->t_height=1.0;
  surf->type=BLOCK_regular;
  surf->obst_surface=1;
  surf->location=0;
  surf->invisible=0;
  surf->transparent=0;
}

/* ------------------ initventsurface ------------------------ */

void initventsurface(surfdata *surf){
  surf->emis=1.0;
  surf->temp_ignition=TEMP_IGNITION_MAX;
  surf->surfacelabel=NULL;
  surf->texturefile=NULL;
  surf->textureinfo=NULL;
  surf->color=ventcolor;
  surf->t_width=1.0;
  surf->t_height=1.0;
  surf->type=BLOCK_outline;
  surf->obst_surface=0;

}

/* ------------------ setsurfaceindex ------------------------ */

void setsurfaceindex(blockagedata *bc){
  int i,j,jj;
  surfdata *surfj;
  char *surflabel, *bclabel;
  int *surf_index;
  int wall_flag;

  for(i=0;i<6;i++){
    bc->surf_index[i]=-1;
    bclabel=bc->surf[i]->surfacelabel;
    if(bc->surf[i]==NULL)continue;
    for(jj=1;jj<nsurfinfo+1;jj++){
      j=jj;
      if(jj==nsurfinfo)j=0;
      surfj = surfinfo + j;
      surflabel=surfj->surfacelabel;
      if(strcmp(bclabel,surflabel)!=0)continue;
      bc->surf_index[i]=j;
      break;
    }
  }
  surf_index = bc->surf_index;
  wall_flag = 1;
  for(i=1;i<6;i++){
    if(surf_index[i]!=surf_index[0]){
      wall_flag=0;
      break;
    }
  }
  if(wall_flag==1){
    bc->walltype=WALL_1;
    bc->walltypeORIG=WALL_1;
    return;
  }
  if(
      surf_index[UP_X]==surf_index[DOWN_X]&&
    surf_index[DOWN_Y]==surf_index[DOWN_X]&&
      surf_index[UP_Y]==surf_index[DOWN_X]
    ){
    bc->walltype=WALL_3;
    bc->walltypeORIG=WALL_3;
    return;
  }
  bc->walltype=WALL_6;
  bc->walltypeORIG=WALL_6;
  
}

/* ------------------ initobst ------------------------ */

void initobst(blockagedata *bc, surfdata *surf,int index,int meshindex){
  int colorindex, blocktype;
  int i;
  char blocklabel[255];
  size_t len;

  bc->prop=NULL;
  bc->is_wuiblock=0;
  bc->transparent=0;
  bc->usecolorindex=0;
  bc->colorindex=-1;
  bc->nshowtime=0;
  bc->hole=0;
  bc->showtime=NULL;
  bc->showhide=NULL;
  bc->showtimelist=NULL;
  bc->show=1;
  bc->id=index;
  bc->meshindex=meshindex;
  bc->hidden=0;
  bc->invisible=0;
  bc->texture_origin[0]=texture_origin[0];
  bc->texture_origin[1]=texture_origin[1];
  bc->texture_origin[2]=texture_origin[2];

  /* 
  new OBST format:
  i1 i2 j1 j2 k1 k2 colorindex blocktype       : if blocktype!=1&&colorindex!=-3
  i1 i2 j1 j2 k1 k2 colorindex blocktype r g b : if blocktype!=1&&colorindex==-3
  i1 i2 j1 j2 k1 k2 ceiling texture blocktype [wall texture floor texture] : if blocktype==1
  int colorindex, blocktype;
  colorindex: -1 default color
              -2 invisible
              -3 use r g b color
              >=0 color/color2/texture index
  blocktype: 0 regular block non-textured
             1 regular block textured
             2 outline
             3 smoothed block
  r g b           colors used if colorindex==-3
  */
  colorindex=-1;
  blocktype=0;
  bc->ijk[IMIN]=0;
  bc->ijk[IMAX]=1;
  bc->ijk[JMIN]=0;
  bc->ijk[JMAX]=1;
  bc->ijk[KMIN]=0;
  bc->ijk[KMAX]=1;
  bc->colorindex=colorindex;
  bc->type=blocktype;

  bc->del=0;
  bc->changed=0;
  bc->changed_surface=0;
  bc->walltype=WALL_1;

  bc->color=surf->color;
  bc->useblockcolor=0;
  for(i=0;i<6;i++){
    bc->surf_index[i]=-1;
    bc->surf[i]=surf;
    bc->faceinfo[i]=NULL;
  }
  for(i=0;i<7;i++){
    bc->patchvis[i]=1;
  }
  sprintf(blocklabel,"**blockage %i",index);
  len=strlen(blocklabel);
  NewMemory((void **)&bc->label,((unsigned int)(len+1))*sizeof(char));
  strcpy(bc->label,blocklabel);
}

/* ------------------ initmesh ------------------------ */

void initmesh(mesh *meshi){

  meshi->iplotx_all=NULL;
  meshi->iploty_all=NULL;
  meshi->iplotz_all=NULL;
#ifdef pp_GPU
  meshi->smoke_texture_buffer=NULL;
  meshi->smoke_texture_id=-1;
  meshi->fire_texture_buffer=NULL;
  meshi->fire_texture_id=-1;
#endif
  meshi->mesh_offset_ptr=NULL;
#ifdef pp_CULL
  meshi->cullinfo=NULL;
  meshi->culldefined=0;
  meshi->cullQueryId=NULL;
  meshi->cull_smoke3d=NULL;
  meshi->smokedir_old=-100;
#endif
  meshi->cullgeominfo=NULL;
  meshi->is_bottom=1;
  meshi->blockvis=1;
  meshi->zcell=NULL;
  meshi->terrain=NULL;
  meshi->meshrgb[0]=0.0;
  meshi->meshrgb[1]=0.0;
  meshi->meshrgb[2]=0.0;
  meshi->meshrgb_ptr=NULL;
  meshi->showsmoothtimelist=NULL;
  meshi->cellsize=0.0;
  meshi->smokeloaded=0;
  meshi->hrrpuv_cutoff=600.0;
  meshi->smokedir=1;
  meshi->merge_alpha=NULL;
  meshi->merge_color=NULL;
  meshi->dx=1.0;
  meshi->dy=1.0;
  meshi->dz=1.0;
  meshi->dxy=1.0;
  meshi->dxz=1.0;
  meshi->dyz=1.0;
  meshi->label=NULL;
  meshi->mxpatch_frames=0;
  meshi->slicedir=2;
  meshi->visInteriorPatches=0;
  meshi->plot3dfilenum=-1;
  meshi->patchfilenum=-1;
  meshi->obst_bysize=NULL;
  meshi->iqdata=NULL;      meshi->qdata=NULL;
  meshi->yzcolorbase=NULL; meshi->xzcolorbase=NULL; meshi->xycolorbase=NULL; 
  meshi->yzcolorfbase=NULL;meshi->xzcolorfbase=NULL;meshi->xycolorfbase=NULL;
  meshi->yzcolortbase=NULL;meshi->xzcolortbase=NULL;meshi->xycolortbase=NULL;
  meshi->dx_xy=NULL;       meshi->dy_xy=NULL;       meshi->dz_xy=NULL;
  meshi->dx_xz=NULL;       meshi->dy_xz=NULL;       meshi->dz_xz=NULL;
  meshi->dx_yz=NULL;       meshi->dy_yz=NULL;       meshi->dz_yz=NULL;
  meshi->c_iblank_xy=NULL; meshi->c_iblank_xz=NULL; meshi->c_iblank_yz=NULL;
  meshi->iblank_smoke3d=NULL;
  meshi->animatedsurfaces=NULL;
  meshi->blockagesurface=NULL;
  meshi->blockagesurfaces=NULL;
  meshi->smoothblockagecolors=NULL;
  meshi->showlevels=NULL;
  meshi->isolevels=NULL;
  meshi->nisolevels=0;
  meshi->iso_times=NULL;
  meshi->iso_timeslist=NULL;
  meshi->isofilenum=-1;
  meshi->nvents=0;
  meshi->ndummyvents=0;
  meshi->npatches=0;
  meshi->patchfacevis2=0;
  meshi->patchtype=NULL;
  meshi->offset[XXX]=0.0;
  meshi->offset[YYY]=0.0;
  meshi->offset[ZZZ]=0.0;
  meshi->patchtype=NULL;
//  meshi->patchfacevis=NULL;
  meshi->patchdir=NULL;
  meshi->patch_surfindex=NULL;
  meshi->pi1=NULL; meshi->pi2=NULL;
  meshi->pj1=NULL; meshi->pj2=NULL; 
  meshi->pk1=NULL; meshi->pk2=NULL;
  meshi->meshonpatch=NULL;
  meshi->blockonpatch=NULL;
  meshi->ptype=NULL;
  meshi->patchrow=NULL, meshi->patchcol=NULL, meshi->blockstart=NULL;
  meshi->zipoffset=NULL, meshi->zipsize=NULL;
  meshi->visPatches=NULL;
  meshi->xyzpatch=NULL;
  meshi->xyzpatch_threshold=NULL;
  meshi->ipqq=NULL, meshi->ipqqi=NULL;
  meshi->ipqqi_zlib=NULL, meshi->ipqq_zlib=NULL;
  meshi->patch_times=NULL, meshi->pqq=NULL, meshi->pqqi=NULL;
  meshi->thresholdtime=NULL;
  meshi->patchblank=NULL;
  meshi->patch_contours=NULL;
  meshi->patch_timeslist=NULL;
  meshi->ntc=0;
  meshi->nspr=0;
  meshi->xsprplot=NULL;
  meshi->ysprplot=NULL;
  meshi->zsprplot=NULL;
  meshi->xspr=NULL;
  meshi->yspr=NULL;
  meshi->zspr=NULL;
  meshi->tspr=NULL;
  meshi->nheat=0;
  meshi->xheatplot=NULL;
  meshi->yheatplot=NULL;
  meshi->zheatplot=NULL;
  meshi->xheat=NULL;
  meshi->yheat=NULL;
  meshi->zheat=NULL;
  meshi->theat=NULL;
  meshi->blockageinfoptrs=NULL;
  meshi->nsmoothblockagecolors=0;

  meshi->surface_tempmax=SURFACE_TEMPMAX;
  meshi->surface_tempmin=SURFACE_TEMPMIN;
 
  meshi->faceinfo=NULL;
  meshi->face_normals_single=NULL;
  meshi->face_normals_double=NULL;
  meshi->face_transparent_double=NULL;
  meshi->face_textures=NULL;
  meshi->face_outlines=NULL;

  meshi->xplt=NULL;
  meshi->yplt=NULL;
  meshi->zplt=NULL;
  meshi->xplt_cen=NULL;
  meshi->yplt_cen=NULL;
  meshi->zplt_cen=NULL;
  meshi->xplt_orig=NULL;
  meshi->yplt_orig=NULL;
  meshi->zplt_orig=NULL;
  meshi->f_iblank_cell=NULL;
  meshi->c_iblank_cell=NULL;
  meshi->c_iblank_x=NULL;
  meshi->c_iblank_y=NULL;
  meshi->c_iblank_z=NULL;

  meshi->xyz_bar[XXX]=1.0;
  meshi->xyz_bar0[XXX]=0.0;
  meshi->xyz_bar[YYY]=1.0;
  meshi->xyz_bar0[YYY]=0.0;
  meshi->xyz_bar[ZZZ]=1.0;
  meshi->xyz_bar0[ZZZ]=0.0;

  meshi->xyzmaxdiff=1.0;

  meshi->xcen=0.5;
  meshi->ycen=0.5;
  meshi->zcen=0.5;

  meshi->plotx=1;
  meshi->ploty=1;
  meshi->plotz=1;

  meshi->boxoffset=0.0;
  meshi->c_iblank_node=NULL;
  meshi->ventinfo=NULL;
  meshi->select_min=0;
  meshi->select_max=0;
}

/* ------------------ freelabels ------------------------ */

void freelabels(flowlabels *flowlabel){
  FREEMEMORY(flowlabel->longlabel);
  FREEMEMORY(flowlabel->shortlabel);
  FREEMEMORY(flowlabel->unit);
}

/* ------------------ createnulllaels ------------------------ */

int createnulllabel(flowlabels *flowlabel){
  char buffer[255];
  size_t len;

  len=strlen("Particles");
  strcpy(buffer,"Particles");
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void **)&flowlabel->longlabel,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->longlabel,buffer);


  len=strlen("Particles");
  strcpy(buffer,"Particles");
  len=strlen(buffer);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void **)&flowlabel->shortlabel,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->shortlabel,buffer);

  len=strlen("Particles");
  strcpy(buffer,"Particles");
  len=strlen(buffer);
  trim(buffer);
  len=strlen(buffer);
  if(NewMemory((void *)&flowlabel->unit,(unsigned int)(len+1))==0)return 2;
  STRCPY(flowlabel->unit,buffer);
  return 0;
}

/* ------------------ readini ------------------------ */

int readini(int scriptconfigfile){
  char smvprogini[1024];
  char *smvprogini_ptr=NULL;

  nlabels=nlabelssmv;
  nticks=ntickssmv;
  strcpy(smvprogini,"");
  if(smokeview_bindir!=NULL)strcat(smvprogini,smokeview_bindir);
  strcat(smvprogini,"smokeview.ini");
  smvprogini_ptr=smokeviewini;
  if(smokeviewini!=NULL){
#ifdef WIN32
    if(STRCMP(smvprogini,smokeviewini)!=0)smvprogini_ptr=smvprogini;
#else
    if(strcmp(smvprogini,smokeviewini)!=0)smvprogini_ptr=smvprogini;
#endif
  }
  
  // check if config files read in earlier were modifed later

  if(is_file_newer(smvprogini_ptr,INIfile)==1){
    printf("*** warning: The config file,\n  %s, is newer than\n  %s \n\n",smvprogini_ptr,INIfile);
  }
  if(is_file_newer(smvprogini_ptr,caseinifilename)==1){
    printf("*** warning: The config file,\n  %s, is newer than\n  %s \n\n",smvprogini_ptr,caseinifilename);
  }
  if(is_file_newer(INIfile,caseinifilename)==1){
    printf("*** warning: The conig file,\n  %s, is newer than\n  %s \n\n",INIfile,caseinifilename);
  }

  // read in config files if they exist

  // smokeview.ini ini in install directory

  if(smvprogini_ptr!=NULL){
    if(readini2(smvprogini_ptr,0)==2)return 2;
    update_terrain_options();
  }

  // smokeview.ini in case directory

  if(INIfile!=NULL){
    if(readini2(INIfile,0)==2)return 2;
  }

  // read in casename.ini

  if(caseinifilename!=NULL){
    if(readini2(caseinifilename,1)==2)return 2;
  }

  // read in ini file specified in script

  if(scriptinifilename2!=NULL&&scriptconfigfile==2){
    if(readini2(scriptinifilename2,1)==2)return 2;
    updatecolors(-1);
    updateshowtitles();
    scriptinifilename2=NULL;
  }
  updateglui();
  if(showall_textures==1)TextureShowMenu(-1);
  if(ncolorbars<=ndefaultcolorbars){
    initdefaultcolorbars();
  }
  updatezoommenu=1;
  getsliceparams2();
  return 0;
}

/* ------------------ readbounini ------------------------ */

void readboundini(void){
  FILE *stream;
  char fullfilename[1024];

  if(boundinifilename==NULL)return;
  strcpy(fullfilename,"");
  if(can_write_to_dir(NULL)==0){
    if(can_write_to_dir(smokeviewtempdir)==0)return;
    strcpy(fullfilename,smokeviewtempdir);
    strcat(fullfilename,dirseparator);
  }
  strcat(fullfilename,boundinifilename);
  stream=fopen(fullfilename,"r");
  if(stream==NULL)return;
  printf("%s",_("reading: "));
  printf("%s\n",fullfilename);

  while(!feof(stream)){
    char buffer[255], buffer2[255];

    CheckMemory;
    if(fgets(buffer,255,stream)==NULL)break;
  
    if(match(buffer,"B_BOUNDARY")==1){
      float gmin, gmax;
      float pmin, pmax;
      int filetype;
      char *buffer2ptr;
      int lenbuffer2;
      int i;

      fgets(buffer,255,stream);
      strcpy(buffer2,"");
      sscanf(buffer,"%f %f %f %f %i %s",&gmin,&pmin,&pmax,&gmax,&filetype,buffer2);
      trim(buffer2);
      buffer2ptr=trim_front(buffer2);
      lenbuffer2=strlen(buffer2ptr);
      for(i=0;i<npatchinfo;i++){
        patchdata *patchi;

        patchi = patchinfo +i;
        if(lenbuffer2!=0&&strcmp(patchi->label.shortlabel,buffer2ptr)==0&&patchi->filetype==filetype&&is_file_newer(boundinifilename,patchi->file)==1){
          bounddata *boundi;

          boundi = &patchi->bounds;
          boundi->defined=1;
          boundi->global_min=gmin;
          boundi->global_max=gmax;
          boundi->percentile_min=pmin;
          boundi->percentile_max=pmax;
        }
      }
      continue;
    }
  }
  return;
}

/* ------------------ writeboundini ------------------------ */

void writeboundini(void){
  FILE *stream=NULL;
  char fullfilename[1024];
  int i;

  if(boundinifilename==NULL)return;
  strcpy(fullfilename,"");
  if(can_write_to_dir(NULL)==0){
    if(can_write_to_dir(smokeviewtempdir)==0)return;
    strcpy(fullfilename,smokeviewtempdir);
    strcat(fullfilename,dirseparator);
  }
  strcat(fullfilename,boundinifilename);

  if(boundinifilename==NULL)return;
  for(i=0;i<npatchinfo;i++){
    bounddata *boundi;
    patchdata *patchi;
    int skipi;
    int j;

    skipi = 0;
    patchi = patchinfo + i;
    if(patchi->bounds.defined==0)continue;
    for(j=0;j<i-1;j++){
      patchdata *patchj;

      patchj = patchinfo + j;
      if(patchi->type==patchj->type&&patchi->filetype==patchj->filetype){
        skipi=1;
        break;
      }
    }
    if(skipi==1)continue;

    boundi = &patchi->bounds;
    if(stream==NULL){
      stream=fopen(fullfilename,"w");
      if(stream==NULL)return;
    }
    fprintf(stream,"B_BOUNDARY\n");
    fprintf(stream," %f %f %f %f %i %s\n",boundi->global_min,boundi->percentile_min,boundi->percentile_max,boundi->global_max,patchi->filetype,patchi->label.shortlabel);
  }
  if(stream!=NULL)fclose(stream);
}

/* ------------------ readini2 ------------------------ */

int readini2(char *inifile, int localfile){
  char buffer[255],buffer2[255];

  int tours_flag;
  int nn;
  int n3d;
  int iplot3d, isetmin, isetmax;
  float p3mintemp, p3maxtemp;
  FILE *stream;
  int i;
  int j;
  int tempval;

  updatemenu=1;
  updatefacelists=1;

  if( (stream=fopen(inifile,"r"))==NULL)return 1;

  for(i=0;i<nunitclasses_ini;i++){
    f_units *uc;

    uc=unitclasses_ini+i;
    FREEMEMORY(uc->units);
  }
  FREEMEMORY(unitclasses_ini);
  nunitclasses_ini=0;

  if(localfile==1){
    update_inilist();
  }

  printf("%s",_("processing config file: "));
  printf("%s\n",inifile);
  if(localfile==1){
    update_selectedtour_index=0;
  }

  /* find number of each kind of file */

  while(!feof(stream)){
    CheckMemory;
    if(fgets(buffer,255,stream)==NULL)break;

    if(match(buffer,"GRIDPARMS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i",&visx_all, &visy_all, &visz_all);
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i",&iplotx_all, &iploty_all, &iplotz_all);
      if(iplotx_all>nplotx_all-1)iplotx_all=0;
      if(iploty_all>nploty_all-1)iploty_all=0;
      if(iplotz_all>nplotz_all-1)iplotz_all=0;
   }
   if(match(buffer,"SHOWDEVICEVALS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i",&showdeviceval,&showvdeviceval,&devicetypes_index);
    }
    if(match(buffer,"USENEWDRAWFACE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&use_new_drawface);
      if(use_new_drawface!=0)use_new_drawface=1;
    }
    if(match(buffer,"TLOAD")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f %i %i",&use_tload_begin,&tload_begin,&use_tload_end,&tload_end,&use_tload_skip,&tload_skip);
    }
    if(match(buffer,"VOLSMOKE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i %i %i",
        &glui_compress_volsmoke,&use_multi_threading,&load_at_rendertimes,&volbw,&show_volsmoke_moving);
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f %f %f",
        &temperature_min,&temperature_cutoff,&temperature_max,&opacity_factor,&mass_extinct);
      if(glui_compress_volsmoke!=0)glui_compress_volsmoke=1;
      if(use_multi_threading!=0)use_multi_threading=1;
      if(load_at_rendertimes!=0)load_at_rendertimes=1;
      opacity_factor=CLAMP(opacity_factor,1.0,10.0);
      mass_extinct=CLAMP(mass_extinct,100.0,100000.0);
      continue;
    }
    if(match(buffer,"MESHOFFSET")==1){
      int meshnum;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&meshnum);
      if(meshnum>=0&&meshnum<nmeshes){
        mesh *meshi;

        meshi = meshinfo + meshnum;
        meshi->mesh_offset_ptr=meshi->mesh_offset;
      }
      continue;
    }
#ifdef pp_LANG
    if(match(buffer,"STARTUPLANG")==1){
      char *bufptr;

      fgets(buffer,255,stream);
      trim(buffer);
      bufptr=trim_front(buffer);
      strncpy(startup_lang_code,bufptr,2);
      startup_lang_code[2]='\0';
      if(strcmp(startup_lang_code,"en")!=0){
        show_lang_menu=1;
      }
      if(tr_name==NULL){
        int langlen;

        langlen=strlen(bufptr);
        NewMemory((void **)&tr_name,langlen+48+1);
        strcpy(tr_name,bufptr);
      }
      continue;
    }
#endif
    if(match(buffer,"MESHVIS")==1){
      int nm;
      mesh *meshi;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&nm);
      for(i=0;i<nm;i++){
        if(i>nmeshes-1)break;
        meshi = meshinfo + i;
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&meshi->blockvis);
        if(meshi->blockvis!=0)meshi->blockvis=1;
      }
      continue;
    }
    if(match(buffer,"SPHERESEGS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&device_sphere_segments);
      if(device_sphere_segments<3)device_sphere_segments=3;
      initspheresegs(device_sphere_segments,2*device_sphere_segments);
      continue;
    }
    if(match(buffer,"SHOWEVACSLICES")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i",&show_evac_slices,&constant_evac_coloring,&show_evac_colorbar);
      if(show_evac_slices!=1)show_evac_slices=0;
      if(constant_evac_coloring!=1)constant_evac_coloring=0;
	  data_evac_coloring=1-constant_evac_coloring;
      if(show_evac_colorbar!=1)show_evac_colorbar=0;
      update_slice_menu_show();
      update_evac_parms();
      continue;
    }
    if(match(buffer,"DIRECTIONCOLOR")==1){
      float *dc;

	  dc = direction_color;
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",dc,dc+1,dc+2);
	  dc[3]=1.0;
	  direction_color_ptr=getcolorptr(direction_color);
      update_slice_menu_show();  
      update_evac_parms();
      continue;
    }
    if(match(buffer,"OFFSETSLICE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&offset_slice);
      if(offset_slice!=0)offset_slice=1;
      continue;
    }
    if(match(buffer,"VECLENGTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&iveclengths);
      vecfactor = get_vecfactor(&iveclengths);
      continue;
    }
    if(match(buffer,"VECTORLENGTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f",&vecfactor);
      continue;
      }
    if(match(buffer,"ISOTRAN2")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&transparent_state);
      continue;
    }
    if(match(buffer,"SHOWTRIANGLES")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i %i %i %i",&showtrisurface,&showtrioutline,&showtripoints,&showtrinormal,&showpointnormal,&smoothtrinormal);
      if(showtrisurface!=0)showtrisurface=1;
      if(showtrioutline!=0)showtrioutline=1;
      if(showtripoints!=0)showtripoints=1;
      if(showtrinormal!=0)showtrinormal=1;
      if(showpointnormal!=0)showpointnormal=1;
      if(smoothtrinormal!=0)smoothtrinormal=1;
#ifdef pp_BETA
      if(showtrinormal!=0)showtrinormal=1;
#else
      showtrinormal=0;
#endif
      visAIso=showtrisurface*1+showtrioutline*2+showtripoints*4;
      continue;
    }
    if(match(buffer,"SHOWSTREAK")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i %i",&streak5show,&streak5step,&showstreakhead,&streak_index);
      if(streak5show!=1){
        streak5show=0;
        streak_index=-2;
      }
      if(showstreakhead!=1)showstreakhead=0;
      update_streaks=1;
      continue;
    }

    if(match(buffer,"SHOWTERRAIN")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visTerrainType);
      continue;
    }
    if(match(buffer,"STEREO")==1){
      fgets(buffer,255,stream);
      showstereoOLD=showstereo;
      sscanf(buffer,"%i",&showstereo);
      if(showstereo<0)showstereo=0;
      if(showstereo>5)showstereo=5;
      if(showstereo==1&&videoSTEREO!=1)showstereo=0;
      Update_Glui_Stereo();
      continue;
    }
    if(match(buffer,"TERRAINPARMS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i",terrain_rgba_zmin,terrain_rgba_zmin+1,terrain_rgba_zmin+2);

      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i",terrain_rgba_zmax,terrain_rgba_zmax+1,terrain_rgba_zmax+2);

      fgets(buffer,255,stream);
      sscanf(buffer,"%f",&vertical_factor);

      for(i=0;i<3;i++){
        if(terrain_rgba_zmin[i]<0)terrain_rgba_zmin[i]=0;
        if(terrain_rgba_zmin[i]>255)terrain_rgba_zmin[i]=255;
        if(terrain_rgba_zmax[i]<0)terrain_rgba_zmax[i]=0;
        if(terrain_rgba_zmax[i]>255)terrain_rgba_zmax[i]=255;
      }
      if(vertical_factor<0.25)vertical_factor=0.25;
      if(vertical_factor>4.0)vertical_factor=4.0;
      update_terrain(0,vertical_factor);
      update_terrain_colors();
    
      continue;
    }
    if(match(buffer,"SMOKESENSORS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i",&show_smokesensors,&test_smokesensors);
      continue;
    }
    if(match(buffer,"SBATSTART")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&sb_atstart);
      if(sb_atstart!=0)sb_atstart=1;
      continue;
    }
#ifdef pp_GPU
    if(gpuactive==1&&match(buffer,"USEGPU")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&usegpu);
      if(usegpu!=0)usegpu=1;
      continue;
    }
#endif
    if(match(buffer,"V_PLOT3D")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&tempval);
      if(tempval<0)tempval=0;
      n3d=tempval;
      if(n3d>mxplot3dvars)n3d=mxplot3dvars;
      for(i=0;i<n3d;i++){  
        fgets(buffer,255,stream);
        sscanf(buffer,"%i %i %f %i %f",&iplot3d,&isetmin,&p3mintemp,&isetmax,&p3maxtemp);
        iplot3d--;
        if(iplot3d>=0&&iplot3d<mxplot3dvars){
          setp3min[iplot3d]=isetmin;
          setp3max[iplot3d]=isetmax;
          p3min[iplot3d]=p3mintemp;
          p3max[iplot3d]=p3maxtemp;
        }
      }
      continue;
    }
    if(match(buffer,"UNLOAD_QDATA")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&unload_qdata);
      if(unload_qdata!=1)unload_qdata=0;
      continue;
    }
    if(match(buffer,"TREECOLORS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",trunccolor,trunccolor+1,trunccolor+2);
      sscanf(buffer,"%f %f %f",treecolor,treecolor+1,treecolor+2);
      sscanf(buffer,"%f %f %f",treecharcolor,treecharcolor+1,treecharcolor+2);
      for(i=0;i<4;i++){
        if(treecolor[i]<0.0)treecolor[i]=0.0;
        if(treecharcolor[i]<0.0)treecharcolor[i]=0.0;
        if(trunccolor[i]<0.0)trunccolor[i]=0.0;
        if(treecolor[i]>1.0)treecolor[i]=1.0;
        if(treecharcolor[i]>1.0)treecharcolor[i]=1.0;
        if(trunccolor[i]>1.0)trunccolor[i]=1.0;
      }
      treecolor[3]=1.0;
      treecharcolor[3]=1.0;
      trunccolor[3]=1.0;
      continue;
    }
    if(match(buffer,"TRAINERVIEW")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&trainerview);
      if(trainerview!=2&&trainerview!=3)trainerview=1;
      continue;
    }
    if(match(buffer,"SMOOTHBLOCKSOLID")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&smooth_block_solid);
      if(smooth_block_solid!=1)smooth_block_solid=0;
      continue;
    }
    if(match(buffer,"SHOWTRANSPARENTVENTS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&show_transparent_vents);
      if(show_transparent_vents!=0)show_transparent_vents=1;
      continue;
    }
    if(match(buffer,"COLORBARTYPE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&colorbartype);
      if(colorbartype!=colorbartype_default){
        colorbartype_ini=colorbartype;
      }
      continue;
    }
    if(match(buffer,"FIRECOLORMAP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i",&use_firesmokemap,&fire_colorbar_index_ini);
      update_fire_colorbar_index=1;
      continue;
    }
    if(match(buffer,"SHOWEXTREMEDATA")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&show_extremedata);
      if(show_extremedata!=1)show_extremedata=0;
      continue;
    }
    if(match(buffer,"EXTREMECOLORS") == 1){
      int mmin[3],mmax[3];

      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i %i %i %i",
        mmin,mmin+1,mmin+2,
        mmax,mmax+1,mmax+2);
      for(i=0;i<3;i++){
        rgb_below_min[i]=CLAMP(mmin[i],0,255);
        rgb_above_max[i]=CLAMP(mmax[i],0,255);
      }
      continue;
    }
    if(match(buffer,"SLICEAVERAGE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i",&slice_average_flag,&slice_average_interval,&vis_slice_average);
      if(slice_average_flag!=1)slice_average_flag=0;
      if(slice_average_interval<0.0)slice_average_interval=0.0;
      continue;
    }
    if(match(buffer,"SKYBOX")==1){
      skyboxdata *skyi;

      free_skybox();
      nskyboxinfo=1;
      NewMemory((void **)&skyboxinfo,nskyboxinfo*sizeof(skyboxdata));
      skyi = skyboxinfo;

      for(i=0;i<6;i++){
        fgets(buffer,255,stream);
        loadskytexture(buffer,skyi->face+i);
      }
    }
    if(match(buffer,"C_PLOT3D")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&tempval);
      if(tempval<0)tempval=0;
      n3d=tempval;
      if(n3d>mxplot3dvars)n3d=mxplot3dvars;
      for(i=0;i<n3d;i++){  
        fgets(buffer,255,stream);
        sscanf(buffer,"%i %i %f %i %f",&iplot3d,&isetmin,&p3mintemp,&isetmax,&p3maxtemp);
        iplot3d--;
        if(iplot3d>=0&&iplot3d<mxplot3dvars){
          setp3chopmin[iplot3d]=isetmin;
          setp3chopmax[iplot3d]=isetmax;
          p3chopmin[iplot3d]=p3mintemp;
          p3chopmax[iplot3d]=p3maxtemp;
        }
      }
      continue;
    }
    if(match(buffer,"DEVICENORMLENGTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f",&devicenorm_length);
      if(devicenorm_length<0.0||devicenorm_length>1.0)devicenorm_length=0.1;
      continue;
    }

    if(match(buffer,"SHOWHRRCUTOFF")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&show_hrrcutoff);
      if(show_hrrcutoff!=1)show_hrrcutoff=0;
      continue;
    }
    if(match(buffer,"TWOSIDEDVENTS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i",&show_bothsides_int,&show_bothsides_ext);
      if(show_bothsides_int!=1)show_bothsides_int=0;
      if(show_bothsides_ext!=1)show_bothsides_ext=0;
      continue;
    }
    if(match(buffer,"SHOWSLICEINOBST")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&show_slice_in_obst);
      if(show_slice_in_obst!=1)show_slice_in_obst=0;
      continue;
    }
    if(match(buffer,"SKIPEMBEDSLICE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&skip_slice_in_embedded_mesh);
      if(skip_slice_in_embedded_mesh!=1)skip_slice_in_embedded_mesh=0;
      continue;
    }
    if(match(buffer,"CELLCENTERINTERP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&cellcenter_interp);
      if(cellcenter_interp!=1)cellcenter_interp=0;
      continue;
    }
    if(match(buffer,"PERCENTILELEVEL")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f",&percentile_level);
      if(percentile_level<0.0)percentile_level=0.01;
      if(percentile_level>0.5)percentile_level=0.01;
      continue;
    }
    if(match(buffer,"TRAINERMODE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&trainer_mode);
      continue;
    }
    if(match(buffer,"COMPRESSAUTO")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&compress_autoloaded);
      continue;
    }
    if(match(buffer,"PLOT3DAUTO")==1){
      int n3dsmokes=0;
      int seq_id;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n3dsmokes);
      for(i=0;i<n3dsmokes;i++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&seq_id);
        get_startup_plot3d(seq_id);
      }
      update_load_startup=1;
      continue;
    }
    if(match(buffer,"VSLICEAUTO")==1){
      int n3dsmokes=0;
      int seq_id;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n3dsmokes);
      for(i=0;i<n3dsmokes;i++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&seq_id);
        get_startup_vslice(seq_id);
      }
      update_load_startup=1;
      continue;
    }
    if(match(buffer,"SLICEAUTO")==1){
      int n3dsmokes=0;
      int seq_id;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n3dsmokes);
      for(i=0;i<n3dsmokes;i++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&seq_id);
        get_startup_slice(seq_id);
      }
      update_load_startup=1;
      continue;
    }
    if(match(buffer,"MSLICEAUTO")==1){
      int n3dsmokes=0;
      int seq_id;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n3dsmokes);
      for(i=0;i<n3dsmokes;i++){

        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&seq_id);

        if(seq_id>=0&&seq_id<nmultislices){
          multislicedata *mslicei;
  
          mslicei = multisliceinfo + seq_id;
          mslicei->autoload=1;
        }
      }
      update_load_startup=1;
      continue;
    }
    if(match(buffer,"PARTAUTO")==1){
      int n3dsmokes=0;
      int seq_id;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n3dsmokes);
      for(i=0;i<n3dsmokes;i++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&seq_id);
        get_startup_part(seq_id);
      }
      update_load_startup=1;
      continue;
    }
    if(match(buffer,"ISOAUTO")==1){
      int n3dsmokes=0;
      int seq_id;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n3dsmokes);
      for(i=0;i<n3dsmokes;i++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&seq_id);
        get_startup_iso(seq_id);
      }
      update_load_startup=1;
      continue;
    }
    if(match(buffer,"S3DAUTO")==1){
      int n3dsmokes=0;
      int seq_id;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n3dsmokes);
      for(i=0;i<n3dsmokes;i++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&seq_id);
        get_startup_smoke(seq_id);
      }
      update_load_startup=1;
      continue;
    }
    if(match(buffer,"PATCHAUTO")==1){
      int n3dsmokes=0;
      int seq_id;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n3dsmokes);
      for(i=0;i<n3dsmokes;i++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&seq_id);
        get_startup_patch(seq_id);
      }
      update_load_startup=1;
      continue;
    }
    if(match(buffer,"LOADFILESATSTARTUP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&loadfiles_at_startup);
      continue;
    }
    if(match(buffer,"SHOWALLTEXTURES")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&showall_textures);
      continue;
    }
    if(match(buffer,"PIXELSKIP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&pixel_skip);
      continue;
    }
    if(match(buffer,"PROJECTION")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&projection_type);
      TRANSLATE_CB(PROJECTION);
      update_projection_type();
      continue;
    }
    if(match(buffer,"V_PARTICLES")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f",&setpartmin,&partmin,&setpartmax,&partmax);
      continue;
    }
    if(match(buffer,"V5_PARTICLES")==1){
      int ivmin, ivmax;
      float vmin, vmax;
      char short_label[256],*s1;

      strcpy(short_label,"");
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f %s",&ivmin,&vmin,&ivmax,&vmax,short_label);
  
      if(npart5prop>0){
        int label_index;
        trim(short_label);
        s1=trim_front(short_label);
        label_index=get_part5prop_index_s(s1);
        if(label_index>=0&&label_index<npart5prop){
          part5prop *propi;

          propi = part5propinfo + label_index;
          propi->setvalmin=ivmin;
          propi->setvalmax=ivmax;
          propi->valmin=vmin;
          propi->valmax=vmax;
          switch (ivmin){
            case PERCENTILE_MIN:
              propi->percentile_min=vmin;
              break;
            case GLOBAL_MIN:
              propi->global_min=vmin;
              break;
            case SET_MIN:
              propi->user_min=vmin;
              break;
            default:
              ASSERT(0);
              break;
          }
          switch (ivmax){
            case PERCENTILE_MAX:
              propi->percentile_max=vmax;
              break;
            case GLOBAL_MAX:
              propi->global_max=vmax;
              break;
            case SET_MAX:
              propi->user_max=vmax;
              break;
            default:
              ASSERT(0);
              break;
          }
        }
      }
      continue;
    }
    if(match(buffer,"C_PARTICLES")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f",&setpartchopmin,&partchopmin,&setpartchopmax,&partchopmax);
      continue;
    }
    if(match(buffer,"V_SLICE")==1){
      float valmin, valmax;
      int setvalmin, setvalmax;

#ifdef pp_SLICECONTOURS
      char *level_val;
#endif

      fgets(buffer,255,stream);
      strcpy(buffer2,"");
      sscanf(buffer,"%i %f %i %f %s",&setvalmin,&valmin,&setvalmax,&valmax,buffer2);
#ifdef pp_SLICECONTOURS
      {
        char *colen;

        colen=strstr(buffer,":");
        level_val=NULL;
        if(colen!=NULL){
          level_val=colen+1;
          trim(level_val);
          *colen=0;
          if(strlen(level_val)>1){
            sscanf(level_val,"%f %f %i",&slice_line_contour_min,&slice_line_contour_max,&slice_line_contour_num);
          }
          {
            level_val=NULL;
          }
        }
      }
#endif
      if(strcmp(buffer2,"")!=0){
        for(i=0;i<nslice2;i++){
          if(strcmp(slicebounds[i].datalabel,buffer2)!=0)continue;
          slicebounds[i].setvalmin=setvalmin;
          slicebounds[i].setvalmax=setvalmax;
          slicebounds[i].valmin=valmin;
          slicebounds[i].valmax=valmax;
#ifdef pp_SLICECONTOURS
          if(level_val!=NULL){
            slicebounds[i].line_contour_min=slice_line_contour_min;  
            slicebounds[i].line_contour_max=slice_line_contour_max;  
            slicebounds[i].line_contour_num=slice_line_contour_num;  
          }
#endif
          break;
        }
      }
      else{
        for(i=0;i<nslice2;i++){
          slicebounds[i].setvalmin=setvalmin;
          slicebounds[i].setvalmax=setvalmax;
          slicebounds[i].valmin=valmin;
          slicebounds[i].valmax=valmax;
#ifdef pp_SLICECONTOURS
          slicebounds[i].line_contour_min=slice_line_contour_min;  
          slicebounds[i].line_contour_max=slice_line_contour_max;  
          slicebounds[i].line_contour_num=slice_line_contour_num;  
#endif
        }
      }
      continue;
    }
    if(match(buffer,"C_SLICE")==1){
      float valmin, valmax;
      int setvalmin, setvalmax;

      fgets(buffer,255,stream);
      strcpy(buffer2,"");
      sscanf(buffer,"%i %f %i %f %s",&setvalmin,&valmin,&setvalmax,&valmax,buffer2);
      if(strcmp(buffer,"")!=0){
        for(i=0;i<nslice2;i++){
          if(strcmp(slicebounds[i].datalabel,buffer2)!=0)continue;
          slicebounds[i].setchopmin=setvalmin;
          slicebounds[i].setchopmax=setvalmax;
          slicebounds[i].chopmin=valmin;
          slicebounds[i].chopmax=valmax;
          break;
        }
      }
      else{
        for(i=0;i<nslice2;i++){
          slicebounds[i].setchopmin=setvalmin;
          slicebounds[i].setchopmax=setvalmax;
          slicebounds[i].chopmin=valmin;
          slicebounds[i].chopmax=valmax;
        }
      }
      continue;
    }
    if(match(buffer,"V_ISO")==1){
      float valmin, valmax;
      int setvalmin, setvalmax;

      fgets(buffer,255,stream);
      strcpy(buffer2,"");
      sscanf(buffer,"%i %f %i %f %s",&setvalmin,&valmin,&setvalmax,&valmax,buffer2);
      if(strcmp(buffer2,"")!=0){
        for(i=0;i<niso_bounds;i++){
          if(strcmp(isobounds[i].datalabel,buffer2)!=0)continue;
          isobounds[i].setvalmin=setvalmin;
          isobounds[i].setvalmax=setvalmax;
          isobounds[i].valmin=valmin;
          isobounds[i].valmax=valmax;
          break;
        }
      }
      else{
        for(i=0;i<niso_bounds;i++){
          isobounds[i].setvalmin=setvalmin;
          isobounds[i].setvalmax=setvalmax;
          isobounds[i].valmin=valmin;
          isobounds[i].valmax=valmax;
        }
      }
      continue;
    }
    if(match(buffer,"C_ISO")==1){
      float valmin, valmax;
      int setvalmin, setvalmax;

      fgets(buffer,255,stream);
      strcpy(buffer2,"");
      sscanf(buffer,"%i %f %i %f %s",&setvalmin,&valmin,&setvalmax,&valmax,buffer2);
      if(strcmp(buffer,"")!=0){
        for(i=0;i<niso_bounds;i++){
          if(strcmp(isobounds[i].datalabel,buffer2)!=0)continue;
          isobounds[i].setchopmin=setvalmin;
          isobounds[i].setchopmax=setvalmax;
          isobounds[i].chopmin=valmin;
          isobounds[i].chopmax=valmax;
          break;
        }
      }
      else{
        for(i=0;i<niso_bounds;i++){
          isobounds[i].setchopmin=setvalmin;
          isobounds[i].setchopmax=setvalmax;
          isobounds[i].chopmin=valmin;
          isobounds[i].chopmax=valmax;
        }
      }
      continue;
    }
    if(match(buffer,"V_BOUNDARY")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f %s",&setpatchmin,&patchmin,&setpatchmax,&patchmax,buffer2);
      if(strcmp(buffer2,"")!=0)local2globalpatchbounds(buffer2);
      continue;
    }
    if(match(buffer,"C_BOUNDARY")==1){
      float valmin, valmax;
      int setvalmin, setvalmax;
      char *buffer2ptr;
      int lenbuffer2;

      fgets(buffer,255,stream);
      strcpy(buffer2,"");
      sscanf(buffer,"%i %f %i %f %s",&setvalmin,&valmin,&setvalmax,&valmax,buffer2);
      trim(buffer2);
      buffer2ptr=trim_front(buffer2);
      lenbuffer2=strlen(buffer2ptr);
      for(i=0;i<npatchinfo;i++){
        if(lenbuffer2==0||strcmp(patchinfo[i].label.shortlabel,buffer2ptr)==0){
          patchinfo[i].chopmin=valmin;
          patchinfo[i].chopmax=valmax;
          patchinfo[i].setchopmin=setvalmin;
          patchinfo[i].setchopmax=setvalmax;
        }
      }
      updatepatchlistindex2(buffer2ptr);
      continue;
    }
    if(match(buffer,"V_ZONE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f",&setzonemin,&zonemin,&setzonemax,&zonemax);
      continue;
    }
    if(match(buffer,"V_TARGET")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f %s",&settargetmin,&targetmin,&settargetmax,&targetmax,buffer2);
      continue;
    }
    if(match(buffer,"OUTLINEMODE")==1){
	    fgets(buffer,255,stream);
	    sscanf(buffer,"%i",&highlight_flag);
      if(nmeshes<2&&highlight_flag!=0)highlight_flag=1;
      continue;
    }
    if(match(buffer,"USENISTLOGO")==1){
	    fgets(buffer,255,stream);
	    sscanf(buffer,"%i",&use_nistlogo);
      continue;
    }
    if(match(buffer,"SLICEDATAOUT")==1){
      {
        int sliceoutflag=0;
  	    fgets(buffer,255,stream);
	      sscanf(buffer,"%i",&sliceoutflag);
        if(sliceoutflag!=0){
          output_slicedata=1;
        }
        else{
          output_slicedata=0;
        }
      }
      continue;
    }
    if(match(buffer,"SMOKE3DZIPSTEP")==1){
	    fgets(buffer,255,stream);
	    sscanf(buffer,"%i",&smoke3dzipstep);
	    if(smoke3dzipstep<1)smoke3dzipstep=1;
      smoke3dzipskip=smoke3dzipstep-1;
      continue;
    }
    if(match(buffer,"SLICEZIPSTEP")==1){
	    fgets(buffer,255,stream);
	    sscanf(buffer,"%i",&slicezipstep);
	    if(slicezipstep<1)slicezipstep=1;
      slicezipskip=slicezipstep-1;
      continue;
    }
    if(match(buffer,"ISOZIPSTEP")==1){
	    fgets(buffer,255,stream);
	    sscanf(buffer,"%i",&isozipstep);
	    if(isozipstep<1)isozipstep=1;
      isozipskip=isozipstep-1;
      continue;
    }
    if(match(buffer,"BOUNDZIPSTEP")==1){
	    fgets(buffer,255,stream);
	    sscanf(buffer,"%i",&boundzipstep);
	    if(boundzipstep<1)boundzipstep=1;
      boundzipskip=boundzipstep-1;
      continue;
    }
    if(match(buffer,"PARTPOINTSTEP")==1){
		  fgets(buffer,255,stream);
		  sscanf(buffer,"%i",&partpointstep);
		  if(partpointstep<1)partpointstep=1;
      partpointskip=partpointstep-1;
      continue;
    }
    if(match(buffer,"MSCALE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",mscale,mscale+1,mscale+2);
      continue;
    }
    if(match(buffer,"CLIP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f",&nearclip,&farclip);
      continue;
    }

    if(match(buffer,"SHOWTRACERSALWAYS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&show_tracers_always);
      if(show_tracers_always!=1)show_tracers_always=0;
      continue;
    }
    if(localfile==1&&match(buffer,"PROPINDEX")==1){
      int nvals;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&nvals);
      for(i=0;i<nvals;i++){
        propdata *propi;
        int ind, val;

        fgets(buffer,255,stream);
        sscanf(buffer,"%i %i",&ind,&val);
        if(ind<0||ind>npropinfo-1)continue;
        propi = propinfo + ind;
        if(val<0||val>propi->nsmokeview_ids-1)continue;
        propi->smokeview_id=propi->smokeview_ids[val];
        propi->smv_object=propi->smv_objects[val];
      }
      for(i=0;i<npartclassinfo;i++){
        part5class *partclassi;

        partclassi = partclassinfo + i;
        update_partclass_depend(partclassi);

      }
    }
    if(localfile==1&&match(buffer,"PART5CLASSVIS")==1){
      int ntemp;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&ntemp);

      for(j=0;j<ntemp;j++){
        part5class *partclassj;

        if(j>npartclassinfo)break;

        partclassj = partclassinfo + j;
        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&partclassj->vis_type);
      }
    }
    if(match(buffer,"PART5COLOR")==1){
      for(i=0;i<npart5prop;i++){
        part5prop *propi;

        propi = part5propinfo + i;
        propi->display=0;
      }
      part5colorindex=0;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&i);
      if(i>=0&&i<npart5prop){
        part5prop *propi;

        part5colorindex=i;
        propi = part5propinfo + i;
        propi->display=1;
      }
      continue;
    }

    if(match(buffer,"PART5PROPDISP")==1){
      char *token;

      for(i=0;i<npart5prop;i++){
        part5prop *propi;

        propi = part5propinfo + i;
        fgets(buffer,255,stream);
    
        trim(buffer);
        token=strtok(buffer," ");
        j=0;
        while(token!=NULL&&j<npartclassinfo){
          int visval;

          sscanf(token,"%i",&visval);
          propi->class_vis[j]=visval;
          token=strtok(NULL," ");
          j++;
        }
      }
      CheckMemory;
      continue;
    }

    if(match(buffer,"COLORBAR") == 1){
      float *rgb_ini_copy;
      
      CheckMemory;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i",&nrgb_ini,&usetexturebar,&colorbar_select_index);
      FREEMEMORY(rgb_ini);
      if(NewMemory((void **)&rgb_ini,4*nrgb_ini*sizeof(float))==0)return 2;
      rgb_ini_copy=rgb_ini;
      for(nn=0;nn<nrgb_ini;nn++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f ",rgb_ini_copy,rgb_ini_copy+1,rgb_ini_copy+2);
        rgb_ini_copy+=3;
      }
      initrgb();
      if(colorbar_select_index>=0&&colorbar_select_index<=255){
        update_colorbar_select_index=1;
      }
      continue;
    }
    if(match(buffer,"COLOR2BAR") == 1){
      float *rgb_ini_copy;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&nrgb2_ini);
      if(nrgb2_ini<8){
        printf("*** fatal error: must have at lease 8 colors in COLOR2BAR\n");
        exit(1);
      }
      FREEMEMORY(rgb2_ini);
      if(NewMemory((void **)&rgb2_ini,4*nrgb_ini*sizeof(float))==0)return 2;
      rgb_ini_copy=rgb2_ini;
      for(nn=0;nn<nrgb2_ini;nn++){
        fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f ",rgb_ini_copy,rgb_ini_copy+1,rgb_ini_copy+2);
        rgb_ini_copy+=3;
      }
      continue;
    }
    if(match(buffer,"P3DSURFACETYPE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&p3dsurfacetype);
      continue;
    }
    if(match(buffer,"P3DSURFACESMOOTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&p3dsurfacesmooth);
      continue;
    }
    if(match(buffer,"CULLFACES")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&cullfaces);
      continue;
    }
    if(match(buffer,"PARTPOINTSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&partpointsize);
      continue;
    }
    if(match(buffer,"ISOPOINTSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&isopointsize);
      continue;
    }
    if(match(buffer,"ISOLINEWIDTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&isolinewidth);
      continue;
    }
    if(match(buffer,"PLOT3DPOINTSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&plot3dpointsize);
      continue;
    }
    if(match(buffer,"PLOT3DLINEWIDTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&plot3dlinewidth);
      continue;
    }
    if(match(buffer,"VECTORPOINTSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&vectorpointsize);
      continue;
    }
    if(match(buffer,"VECTORLINEWIDTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&vectorlinewidth);
      continue;
    }
    if(match(buffer,"STREAKLINEWIDTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&streaklinewidth);
      continue;
    }
    if(match(buffer,"LINEWIDTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&linewidth);
      solidlinewidth=linewidth;
      continue;
    }
    if(match(buffer,"VENTLINEWIDTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&ventlinewidth);
      continue;
    }
    if(match(buffer,"SLICEOFFSET")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&sliceoffset_factor);
      continue;
    }
    if(match(buffer,"TITLESAFE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&titlesafe_offset);
      continue;
    }
    if(match(buffer,"VENTOFFSET")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&ventoffset_factor);
      continue;
    }
    if(match(buffer,"AXISSMOOTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&axislabels_smooth);
      continue;
    }
    if(match(buffer,"SHOWBLOCKS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visBlocks);
      continue;
    }
    if(match(buffer,"SHOWSENSORS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i ",&visSensor,&visSensorNorm);
      if(visSensor!=0)visSensor=1;
      if(visSensorNorm!=0)visSensorNorm=1;
      continue;
    }
    if(match(buffer,"AVATAREVAC")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&iavatar_evac);
      continue;
    }
    if(match(buffer,"SHOWVENTS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i ",&visVents,&visVentLines,&visVentSolid);
      if(visVents==0){
        visVentLines=0;
        visVentSolid=0;
      }
      if(visVentSolid==1)visVentLines=0;
      if(visVentLines==1)visVentSolid=0;
      continue;
    }
    if(match(buffer,"SHOWTIMELABEL")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visTimelabel);
      continue;
    }
    if(match(buffer,"SHOWHMSTIMELABEL")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&vishmsTimelabel);
      continue;
    }
    if(match(buffer,"SHOWFRAMELABEL")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visFramelabel);
      continue;
    }
    if(match(buffer,"SHOWHRRLABEL")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visHRRlabel);
      continue;
    }
    if(match(buffer,"RENDERFILETYPE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&renderfiletype);
#ifndef pp_JPEG
      if(renderfiletype==1){
        printf("*** warning: JPEG not supported, render filetype changed to PNG\n");
        renderfiletype=0;
      }
#endif
      continue;
    }
    if(match(buffer,"RENDERFILELABEL")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&renderfilelabel);
      if(renderfilelabel!=0)renderfilelabel=1;
      continue;
    }
    if(match(buffer,"SHOWGRIDLOC")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visgridloc);
      continue;
    }
    if(match(buffer,"SHOWFLOOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visFloor);
      continue;
    }
    if(match(buffer,"SPEED")==1){
      fgets(buffer,255,stream);
    //  sscanf(buffer,"%f %f",&speed_crawl,&speed_walk);
      continue;
    }
    if(match(buffer,"FONTSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&fontindex);
      if(fontindex<0)fontindex=0;
      if(fontindex>1)fontindex=1;
      FontMenu(fontindex);
      continue;
    }
    if(match(buffer,"ZOOM")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f ",&zoomindex,&zoom);
      if(zoomindex!=-1){
        if(zoomindex<0)zoomindex=2;
        if(zoomindex>4)zoomindex=2;
        zoom=zooms[zoomindex];
      }
      else{
        if(zoom<zooms[0]){
          zoom=zooms[0];
          zoomindex=0;
        }
        if(zoom>zooms[4]){
          zoom=zooms[4];
          zoomindex=4;
        }
      }
      zoomini=zoom;
      updatezoomini=1;
      ZoomMenu(zoomindex);
      continue;
    }
    if(match(buffer,"APERATURE")==1||match(buffer,"APERTURE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&apertureindex);
      if(apertureindex<0)apertureindex=0;
      if(apertureindex>4)apertureindex=4;
      ApertureMenu(apertureindex);
      continue;
    }
    if(match(buffer,"SHOWWALLS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visWalls);
      continue;
      }
    if(match(buffer,"SHOWCEILING")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visCeiling);
      continue;
      }
    if(match(buffer,"SHOWTITLE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visTitle0);
      continue;
      }
    if(match(buffer,"SHOWNORMALWHENSMOOTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visSmoothAsNormal);
      continue;
      }
    if(match(buffer,"SHOWTRANSPARENT")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&visTransparentBlockage);
      continue;
      }
    if(match(buffer,"VECTORPOINTSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&vectorpointsize);
      continue;
      }
    if(match(buffer,"VECTORSKIP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&vectorskip);
      if(vectorskip<1)vectorskip=1;
      continue;
      }
    if(match(buffer,"SPRINKLERABSSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&sprinklerabssize);
      continue;
      }
    if(match(buffer,"SENSORABSSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&sensorabssize);
      continue;
      }
    if(match(buffer,"SENSORRELSIZE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f ",&sensorrelsize);
      if(sensorrelsize<sensorrelsizeMIN)sensorrelsize=sensorrelsizeMIN;
      continue;
      }
    if(match(buffer,"SETBW")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&setbw);
      continue;
      }
    if(match(buffer,"FLIP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&background_flip);
      continue;
      }
    if(match(buffer,"COLORBARFLIP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&colorbarflip);
      continue;
      }
    if(match(buffer,"TRANSPARENT")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f",&use_transparency_data,&transparent_level);
      continue;
      }
    if(match(buffer,"VENTCOLOR")==1){
      float ventcolor_temp[4];

      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",ventcolor_temp,ventcolor_temp+1,ventcolor_temp+2);
      ventcolor_temp[3]=1.0;
      ventcolor=getcolorptr(ventcolor_temp);
      updatefaces=1;
      updateindexcolors=1;
      continue;
      }
    if(match(buffer,"STATICPARTCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",static_color,static_color+1,static_color+2);
      continue;
      }
    if(match(buffer,"HEATOFFCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",heatoffcolor,heatoffcolor+1,heatoffcolor+2);
      continue;
      }
    if(match(buffer,"HEATONCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",heatoncolor,heatoncolor+1,heatoncolor+2);
      continue;
      }
    if(match(buffer,"SENSORCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",sensorcolor,sensorcolor+1,sensorcolor+2);
      continue;
      }
    if(match(buffer,"SENSORNORMCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",sensornormcolor,sensornormcolor+1,sensornormcolor+2);
      continue;
      }
    if(match(buffer,"SPRINKONCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",sprinkoncolor,sprinkoncolor+1,sprinkoncolor+2);
      continue;
      }
    if(match(buffer,"SPRINKOFFCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",sprinkoffcolor,sprinkoffcolor+1,sprinkoffcolor+2);
      continue;
      }
    if(match(buffer,"BACKGROUNDCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",backgroundbasecolor,backgroundbasecolor+1,backgroundbasecolor+2);
      continue;
      }
    if(match(buffer,"FOREGROUNDCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",foregroundbasecolor,foregroundbasecolor+1,foregroundbasecolor+2);
      continue;
      }
    if(match(buffer,"BLOCKCOLOR")==1){
      float blockcolor_temp[4];

      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",blockcolor_temp,blockcolor_temp+1,blockcolor_temp+2);
      blockcolor_temp[3]=1.0;
      block_ambient2=getcolorptr(blockcolor_temp);
      updatefaces=1;
      updateindexcolors=1;
      continue;
      }
    if(match(buffer,"BLOCKLOCATION")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&blocklocation);
      continue;
      }
    if(match(buffer,"SHOWOPENVENTS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i",&visOpenVents,&visOpenVentsAsOutline);
      continue;
      }
    if(match(buffer,"SHOWDUMMYVENTS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visDummyVents);
      continue;
      }
    if(match(buffer,"SHOWOTHERVENTS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visOtherVents);
      continue;
      }
    if(match(buffer,"SHOWTICKS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visTicks);
      continue;
      }
    if(match(buffer,"USERTICKS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i %i %i %i",&vis_user_ticks,&auto_user_tick_placement,&user_tick_sub,
        &user_tick_show_x,&user_tick_show_y,&user_tick_show_z);
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",user_tick_origin,user_tick_origin+1,user_tick_origin+2);
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",user_tick_min,user_tick_min+1,user_tick_min+2);
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",user_tick_max,user_tick_max+1,user_tick_max+2);
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",user_tick_step,user_tick_step+1,user_tick_step+2);
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %i %i",&user_tick_show_x,&user_tick_show_y,&user_tick_show_z);
      continue;
      }
    if(match(buffer,"SHOWLABELS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visLabels);
      continue;
      }
    if(match(buffer,"BOUNDCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",boundcolor,boundcolor+1,boundcolor+2);
      continue;
      }
    if(match(buffer,"TIMEBARCOLOR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",timebarcolor,timebarcolor+1,timebarcolor+2);
      continue;
      }
    if(match(buffer,"CONTOURTYPE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&contour_type);
      contour_type=CLAMP(contour_type,0,2);
      continue;
      }
    if(match(buffer,"P3VIEW")==1){
      for(i=0;i<nmeshes;i++){
        mesh *meshi;

        meshi = meshinfo + i;
        fgets(buffer,255,stream);
        sscanf(buffer,"%i %i %i %i %i %i",
          &visx_all,&meshi->plotx,&visy_all,&meshi->ploty,&visz_all,&meshi->plotz);
        if(visx_all!=0)visx_all=1;
        if(visy_all!=0)visy_all=1;
        if(visy_all!=0)visz_all=1;
        if(meshi->plotx<0)meshi->plotx=0;
        if(meshi->plotx>meshi->ibar)meshi->plotx=meshi->ibar;
        if(meshi->ploty>meshi->jbar)meshi->ploty=meshi->jbar;
        if(meshi->plotz>meshi->kbar)meshi->plotz=meshi->kbar;
      }
      continue;
      }
    if(match(buffer,"P3CONT3DSMOOTH")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&p3cont3dsmooth);
      continue;
      }
    if(match(buffer,"SURFINC")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&surfincrement);
      continue;
      }
    if(match(buffer,"FRAMERATE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visFramerate);
      continue;
      }
    if(match(buffer,"SHOWFRAMERATE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visFramerate);
      continue;
      }
    if(match(buffer,"SHOWFRAME")==1&&match(buffer,"SHOWFRAMERATE")!=1&&match(buffer,"SHOWFRAMELABEL")!=1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visFrame);
      if(isZoneFireModel==1)visFrame=0;
      continue;
    }
    if(match(buffer,"FRAMERATEVALUE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&frameratevalue);
      FrameRateMenu(frameratevalue);
      continue;
      }
    if(match(buffer,"SHOWSPRINKPART")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visSprinkPart);
      continue;
      }
    if(match(buffer,"SHOWAXISLABELS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visaxislabels);
      continue;
      }
#ifdef pp_memstatus
    if(match(buffer,"SHOWMEMLOAD")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visAvailmemory);
      continue;
      }
#endif
    if(match(buffer,"SHOWBLOCKLABEL")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visBlocklabel);
      continue;
      }
    if(match(buffer,"SHOWVZONE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visVZone);
      continue;
      }
    if(match(buffer,"SHOWZONEFIRE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i ",&viszonefire);
      if(viszonefire!=0)viszonefire=1;
      continue;
    }
    if(match(buffer,"SHOWSZONE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visSZone);
      continue;
      }
    if(match(buffer,"SHOWHZONE")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visHZone);
      continue;
      }
    if(match(buffer,"SHOWHAZARDCOLORS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&sethazardcolor);
      continue;
      }
    if(match(buffer,"SHOWSMOKEPART")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visSmokePart);
      continue;
      }
    if(match(buffer,"RENDEROPTION")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&render_option);
      RenderMenu(render_option);
      continue;
      }
    if(match(buffer,"SHOWISONORMALS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&showtrinormal);
      if(showtrinormal!=1)showtrinormal=0;
      continue;
    }
    if(match(buffer,"SHOWISO")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visAIso);
      visAIso&=7;
      showtrisurface=(visAIso&1)/1;
      showtrioutline=(visAIso&2)/2;
      showtripoints=(visAIso&4)/4;
      continue;
      }
    if(trainer_mode==0&&windowresized==0){
      if(match(buffer,"WINDOWWIDTH")==1){
        int scrWidth;

        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&scrWidth);
        if(scrWidth<=0){
          scrWidth = glutGet(GLUT_SCREEN_WIDTH);
        }
        if(scrWidth!=screenWidth){
          screenWidth=scrWidth;
          update_screensize=1;
        }
        continue;
        }
      if(match(buffer,"WINDOWHEIGHT")==1){
        int scrHeight;

        fgets(buffer,255,stream);
        sscanf(buffer,"%i",&scrHeight);
        if(scrHeight<=0){
          scrHeight = glutGet(GLUT_SCREEN_HEIGHT);
        }
        if(scrHeight!=screenHeight){
          screenHeight=scrHeight;
          update_screensize=1;
        }
        continue;
        }
    }
    if(match(buffer,"SHOWTIMEBAR")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visTimeLabels);
      continue;
      }
    if(match(buffer,"SHOWCOLORBARS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visColorbarLabels);
      continue;
      }
    if(match(buffer,"EYEVIEW")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&eyeview);
      continue;
      }
    if(localfile==1&&match(buffer,"SCRIPTFILE")==1){
      if(fgets(buffer2,255,stream)==NULL)break;
      cleanbuffer(buffer,buffer2);
      insert_scriptfile(buffer);
      updatemenu=1;
      continue;
      }
    if(localfile==1&&match(buffer,"SHOWDEVICES")==1){
      sv_object *obj_typei;
      char *dev_label;
      int ndevices_ini;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&ndevices_ini);

      for(i=0;i<nobject_defs;i++){
        obj_typei = object_defs[i];
        obj_typei->visible=0;
      }
      for(i=0;i<ndevices_ini;i++){
        fgets(buffer,255,stream);
        trim(buffer);
        dev_label=trim_front(buffer);
        obj_typei=get_object(dev_label);
        if(obj_typei!=NULL){
          obj_typei->visible=1;
        }
      }
    }
    if(localfile==1&&match(buffer,"XYZCLIP")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&xyz_clipplane);
      if(xyz_clipplane<0)xyz_clipplane=0;
      if(xyz_clipplane>2)xyz_clipplane=2;
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f",&clip_x, &clip_x_val, &clip_X, &clip_X_val);
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f",&clip_y, &clip_y_val, &clip_Y, &clip_Y_val);
      fgets(buffer,255,stream);
      sscanf(buffer,"%i %f %i %f",&clip_z, &clip_z_val, &clip_Z, &clip_Z_val);
      updateclipvals=1;
      continue;
      }
    if(match(buffer,"NOPART")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&nopart);
      continue;
      }
    if(match(buffer,"WINDOWOFFSET")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&titlesafe_offsetBASE);
      continue;
      }
    if(match(buffer,"SHOWLIGHT0")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visLIGHT0);
      UpdateLIGHTS=1;
      continue;
    }
    if(match(buffer,"SHOWLIGHT1")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&visLIGHT1);
      UpdateLIGHTS=1;
      continue;
    }
    if(match(buffer,"SHOWLIGHTMENU")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&showlightmenu);
      updatemenu=1;
      continue;
    }
    if(match(buffer,"AMBIENTLIGHT")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",ambientlight,ambientlight+1,ambientlight+2);
      UpdateLIGHTS=1;
      continue;
    }
    if(match(buffer,"DIFFUSELIGHT")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",diffuselight,diffuselight+1,diffuselight+2);
      UpdateLIGHTS=1;
      continue;
    }
    if(match(buffer,"LABELSTARTUPVIEW")==1){
      char *front;

      fgets(buffer,255,stream);
      front=trim_front(buffer);
      trim(front);
      strcpy(label_startup_view,front);
      updategluiview=1;
      continue;
    }
    if(match(buffer,"INPUT_FILE") == 1){
      {
        size_t len;

        if(fgets(buffer,255,stream)==NULL)break;
        len=strlen(buffer);
        buffer[len-1]='\0';
        trim(buffer);
        len=strlen(buffer);
 
        FREEMEMORY(INI_fds_filein);
        if(NewMemory((void **)&INI_fds_filein,(unsigned int)(len+1))==0)return 2;
        STRCPY(INI_fds_filein,buffer);
        continue;
      }

    }


      {
        float *eye,*mat,*angle_zx;
        int is_viewpoint4=0;
        int is_viewpoint5=0;

    if(match(buffer,"VIEWPOINT3")==1
      ||match(buffer,"VIEWPOINT4")==1
      ||match(buffer,"VIEWPOINT5")==1
      ){
        int p_type;

        if(match(buffer,"VIEWPOINT4")==1)is_viewpoint4=1;
        if(match(buffer,"VIEWPOINT5")==1){
          is_viewpoint4=1;
          is_viewpoint5=1;
        }
        eye=camera_ini->eye;
        mat=camera_ini->modelview;
        angle_zx=camera_ini->angle_zx;

        {
          char name_ini[32];
          strcpy(name_ini,"ini");
          init_camera(camera_ini,name_ini);
        }

		    fgets(buffer,255,stream);
		    sscanf(buffer,"%i %i %i",&camera_ini->eyeview,&camera_ini->rotation_index,&camera_ini->view_id);

        {
          float zoom_in;
          int zoomindex_in;

          zoom_in=zoom;
          zoomindex_in=zoomindex;
          fgets(buffer,255,stream);
	  	    sscanf(buffer,"%f %f %f %f %i",eye,eye+1,eye+2,&zoom_in,&zoomindex_in);
          zoom=zoom_in;
          zoomindex=zoomindex_in;
          if(zoomindex!=-1){
           if(zoomindex<0)zoomindex=2;
           if(zoomindex>4)zoomindex=2;
           zoom=zooms[zoomindex];
          }
          else{
            if(zoom<zooms[0]){
              zoom=zooms[0];
              zoomindex=0;
            }
            if(zoom>zooms[4]){
              zoom=zooms[4];
              zoomindex=4;
            }
          }
          updatezoommenu=1;
        }

        p_type=0;
		    fgets(buffer,255,stream);
		    sscanf(buffer,"%f %f %f %i",
          &camera_ini->view_angle, 
          &camera_ini->direction_angle,
          &camera_ini->elevation_angle,
          &p_type);
        if(p_type!=1)p_type=0;
        camera_ini->projection_type=p_type;

		    fgets(buffer,255,stream);
		    sscanf(buffer,"%f %f %f",&camera_ini->xcen,&camera_ini->ycen,&camera_ini->zcen);

		    fgets(buffer,255,stream);
        sscanf(buffer,"%f %f",angle_zx,angle_zx+1);

		    fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f %f",mat,mat+1,mat+2,mat+3);

		    fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f %f",mat+4,mat+5,mat+6,mat+7);

		    fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f %f",mat+8,mat+9,mat+10,mat+11);

		    fgets(buffer,255,stream);
        sscanf(buffer,"%f %f %f %f",mat+12,mat+13,mat+14,mat+15);
        if(is_viewpoint5==1){
          camera *ci;

          ci = camera_ini;
  		    fgets(buffer,255,stream);
          sscanf(buffer,"%i %i %i %i %i %i %i",
            &ci->xyz_clipplane,
            &ci->clip_x,&ci->clip_y,&ci->clip_z,
            &ci->clip_X,&ci->clip_Y,&ci->clip_Z);
  		    fgets(buffer,255,stream);
          sscanf(buffer,"%f %f %f %f %f %f",
            &ci->clip_x_val,&ci->clip_y_val,&ci->clip_z_val,
            &ci->clip_X_val,&ci->clip_Y_val,&ci->clip_Z_val);

        }
        if(is_viewpoint4==1){
          char *bufferptr;
          
  		    fgets(buffer,255,stream);
          trim(buffer);
          bufferptr=trim_front(buffer);
          strcpy(camera_ini->name,bufferptr);
          init_camera_list();
          {
            //*** following code shouldn't be here but leaving it here commented
            //    in case it is really necessary

            //  camera *cam;

            insert_camera(&camera_list_first,camera_ini,bufferptr);
           // if(cam!=NULL){
           //   cam->view_id=camera_ini->view_id;
           // }
          }
        }

        enable_reset_saved_view();
        camera_ini->dirty=1;
        camera_ini->defined=1;
        continue;
      }
    }

    if(match(buffer,"ISOCOLORS")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f",&iso_shininess,&iso_transparency);
      fgets(buffer,255,stream);
      sscanf(buffer,"%f %f %f",iso_specular,iso_specular+1,iso_specular+2);
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&n_iso_ambient_ini);
      if(n_iso_ambient_ini>0){
        FREEMEMORY(iso_ambient_ini);
        if(NewMemory((void**)&iso_ambient_ini,n_iso_ambient_ini*4*sizeof(float))==0)return 2;
        for(nn=0;nn<n_iso_ambient_ini;nn++){
          fgets(buffer,255,stream);
          sscanf(buffer,"%f %f %f",iso_ambient_ini+4*nn,iso_ambient_ini+4*nn+1,iso_ambient_ini+4*nn+2);
          iso_ambient_ini[4*nn+3]=iso_transparency;
        }
        iso_ambient_ini[3]=1.0;
      }
      update_isocolors();
      continue;
    }
    if(match(buffer,"UNITCLASSES")==1){
      int nuc;

      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&nuc);
      for(i=0;i<nuc;i++){
        int active;

        fgets(buffer,255,stream);
        if(i>nunitclasses-1)continue;
        sscanf(buffer,"%i",&active);
        unitclasses[i].active=active;
      }
      continue;
    }
    if(match(buffer,"SMOOTHLINES")==1){
      fgets(buffer,255,stream);
      sscanf(buffer,"%i",&antialiasflag);
      continue;
      }
    if(localfile==1&&match(buffer,"VIEWTIMES") == 1){
      if(fgets(buffer,255,stream)==NULL)break;
      sscanf(buffer,"%f %f %i",&view_tstart,&view_tstop,&view_ntimes);
      if(view_ntimes<2)view_ntimes=2;
      ReallocTourMemory();
      continue;
    }
#ifdef pp_SHOOTER
    if(localfile==1&&match(buffer,"SHOOTER") == 1){
      if(fgets(buffer,255,stream)==NULL)break;
      sscanf(buffer,"%f %f %f",shooter_xyz,shooter_xyz+1,shooter_xyz+2);
      
      if(fgets(buffer,255,stream)==NULL)break;
      sscanf(buffer,"%f %f %f",shooter_dxyz,shooter_dxyz+1,shooter_dxyz+2);
      
      if(fgets(buffer,255,stream)==NULL)break;
      sscanf(buffer,"%f %f %f",shooter_uvw,shooter_uvw+1,shooter_uvw+2);

      if(fgets(buffer,255,stream)==NULL)break;
      sscanf(buffer,"%f %f %f",&shooter_velmag,&shooter_veldir,&shooterpointsize);
      
      if(fgets(buffer,255,stream)==NULL)break;
      sscanf(buffer,"%i %i %i %i %i",&shooter_fps,&shooter_vel_type,&shooter_nparts,&visShooter,&shooter_cont_update);

      if(fgets(buffer,255,stream)==NULL)break;
      sscanf(buffer,"%f %f",&shooter_duration,&shooter_v_inf);
      continue;
    }
#endif
    {
      int nkeyframes;
      float key_time, key_xyz[3], key_az_path, key_view[3], params[3], zzoom, key_elev_path;
      float t_globaltension, key_bank;
      int t_globaltension_flag;
      int viewtype,uselocalspeed;
      float *col;

      if(match(buffer,"ADJUSTALPHA")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i",&adjustalphaflag);
        continue;
      }
      if(match(buffer,"SMOKECULL")==1){
        if(fgets(buffer,255,stream)==NULL)break;
#ifdef pp_CULL
        if(gpuactive==1){
          sscanf(buffer,"%i",&cullsmoke);
          if(cullsmoke!=0)cullsmoke=1;
        }
        else{
          cullsmoke=0;
        }
#else
        sscanf(buffer,"%i",&smokecullflag);
#endif
        continue;
      }
      if(match(buffer,"SMOKESKIP")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i",&smokeskipm1);
        continue;
      }
      if(match(buffer,"SMOKEALBEDO")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f",&smoke_shade);
        smoke_shade=0.0;
        continue;
      }
      if(match(buffer,"SMOKETHICK")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i",&smoke3d_thick);
        continue;
      }
#ifdef pp_GPU
      if(match(buffer,"SMOKERTHICK")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f",&smoke3d_rthick);
        if(smoke3d_rthick<1.0)smoke3d_rthick=1.0;
        if(smoke3d_rthick>255.0)smoke3d_rthick=255.0;
        smoke3d_thick=log_base2(smoke3d_rthick);
        continue;
      }
#endif
      if(match(buffer,"FIRECOLOR")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i %i %i",&fire_red,&fire_green,&fire_blue);
        continue;
      }
      if(match(buffer,"FIREDEPTH")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f",&fire_halfdepth);
        continue;
      }


      if(match(buffer,"VIEWTOURFROMPATH")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i",&viewtourfrompath);
        continue;
      }
      if(match(buffer,"VIEWALLTOURS")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i",&viewalltours);
        continue;
      }
      if(match(buffer,"SHOWTOURROUTE")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i",&edittour);
        continue;
      }
      if(match(buffer,"TIMEOFFSET")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f",&timeoffset);
        continue;
      }
      if(match(buffer,"SHOWPATHNODES")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i",&show_path_knots);
        continue;
      }
      if(match(buffer,"SHOWIGNITION")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i %i",&vis_threshold,&vis_onlythreshold);
        continue;
      }
      if(match(buffer,"SHOWTHRESHOLD")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i %i %f",&vis_threshold,&vis_onlythreshold,&temp_threshold);
        continue;
      }
      if(match(buffer,"TOURCONSTANTVEL")==1){
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%i",&tour_constant_vel);
        continue;
      }
      if(match(buffer,"TOUR_AVATAR")==1){
        if(fgets(buffer,255,stream)==NULL)break;
//        sscanf(buffer,"%i %f %f %f %f",&tourlocus_type,tourcol_avatar,tourcol_avatar+1,tourcol_avatar+2,&tourrad_avatar);
//        if(tourlocus_type!=0)tourlocus_type=1;
        continue;
      }

  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ GENCOLORBAR ++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

      if(match(buffer,"GCOLORBAR") == 1){
        colorbardata *cbi;
        int r1, g1, b1;
        int n;
        int ncolorbarini;

        fgets(buffer,255,stream);
        ncolorbarini=0;
        sscanf(buffer,"%i",&ncolorbarini);

        initdefaultcolorbars();

        ncolorbars=ndefaultcolorbars+ncolorbarini;
        if(ncolorbarini>0)ResizeMemory((void **)&colorbarinfo,ncolorbars*sizeof(colorbardata));

        for(n=ndefaultcolorbars;n<ncolorbars;n++){
          cbi = colorbarinfo + n;

          fgets(buffer,255,stream);
          trim(buffer);
          strcpy(cbi->label,buffer);

          fgets(buffer,255,stream);
          sscanf(buffer,"%i %i",&cbi->nnodes,&cbi->nodehilight);
          if(cbi->nnodes<0)cbi->nnodes=0;
          if(cbi->nodehilight<0||cbi->nodehilight>=cbi->nnodes){
            cbi->nodehilight=0;
          }

          cbi->label_ptr=cbi->label;
          for(i=0;i<cbi->nnodes;i++){
            int icbar;

            fgets(buffer,255,stream);
            r1=-1; g1=-1; b1=-1; 
            sscanf(buffer,"%i %i %i %i",&icbar,&r1,&g1,&b1);
            cbi->index_node[i]=icbar;
            nn = 3*i;
            cbi->rgb_node[nn  ]=r1;
            cbi->rgb_node[nn+1]=g1;
            cbi->rgb_node[nn+2]=b1;
          }

          remapcolorbar(cbi);
        }
    }

      if(match(buffer,"TOURCOLORS")==1){
        col=tourcol_selectedpathline;
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f %f %f",col,col+1,col+2);

        col=tourcol_selectedpathlineknots;
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f %f %f",col,col+1,col+2);

        col=tourcol_selectedknot;
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f %f %f",col,col+1,col+2);

        col=tourcol_pathline;
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f %f %f",col,col+1,col+2);

        col=tourcol_pathknots;
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f %f %f",col,col+1,col+2);

        col=tourcol_text;
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f %f %f",col,col+1,col+2);

        col=tourcol_avatar;
        if(fgets(buffer,255,stream)==NULL)break;
        sscanf(buffer,"%f %f %f",col,col+1,col+2);

        continue;
      }

  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ LABEL ++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

    if(localfile==1&&match(buffer,"LABEL") == 1){
      nlabels++;
      ResizeMemory((void **)&labelinfo,(nlabels)*sizeof(labeldata));

      /*
      LABEL
      x y z r g b tstart tstop  
      label

      */
      {
        float *xyz, *rgbtemp, *tstart_stop;
        labeldata *labeli;

        labeli = labelinfo + nlabels-1;

        xyz = labeli->xyz;
        rgbtemp = labeli->rgb;
        tstart_stop = labeli->tstart_stop;

        fgets(buffer,255,stream);
        rgbtemp[0]=-1.0;
        rgbtemp[1]=-1.0;
        rgbtemp[2]=-1.0;
        rgbtemp[3]=1.0;
        tstart_stop[0]=-1.0;
        tstart_stop[1]=-1.0;
        sscanf(buffer,"%f %f %f %f %f %f %f %f",
          xyz,xyz+1,xyz+2,
          rgbtemp,rgbtemp+1,rgbtemp+2,
          tstart_stop,tstart_stop+1);
        if(rgbtemp[0]<0.0||rgbtemp[1]<0.0||rgbtemp[2]<0.0||rgbtemp[0]>1.0||rgbtemp[1]>1.0||rgbtemp[2]>1.0){
          labeli->useforegroundcolor=1;
        }
        else{
          labeli->useforegroundcolor=0;
        }
        fgets(buffer,255,stream);
        strcpy(labeli->label,buffer);
      }
      continue;
    }

  /*
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    ++++++++++++++++++++++ TICKS ++++++++++++++++++++++++++++++++
    +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  */

/*
typedef struct {
  float begin[3],end[3],length;
  float dxyz[3],dlength;
  int dir,nbars;
} tickdata;
*/

    if(localfile==1&&match(buffer,"TICKS") == 1){
      nticks++;
      ResizeMemory((void **)&tickinfo,(nticks)*sizeof(tickdata));

      {
        tickdata *ticki;
        float *begt, *endt;
        int *nbarst;
        float term;
        float length=0.0;
        float *dxyz;
        float sum;

        ticki = tickinfo + nticks - 1;
        begt = ticki->begin;
        endt = ticki->end;
        nbarst=&ticki->nbars;
        dxyz = ticki->dxyz;
        

        /*
        TICKS
        b1 b2 b3 e1 e2 e3 nb
        ticklength tickdir tickcolor (r g b) tickwidth 
        */
        if(fgets(buffer,255,stream)==NULL)break;
        *nbarst=0;
        sscanf(buffer,"%f %f %f %f %f %f %i",begt,begt+1,begt+2,endt,endt+1,endt+2,nbarst);
        if(*nbarst<1)*nbarst=1;
        if(fgets(buffer,255,stream)==NULL)break;
        {
          float *rgbtemp;

          rgbtemp=ticki->rgb;
          rgbtemp[0]=-1.0;
          rgbtemp[1]=-1.0;
          rgbtemp[2]=-1.0;
          ticki->width=-1.0;
          sscanf(buffer,"%f %i %f %f %f %f",&ticki->dlength,&ticki->dir,rgbtemp,rgbtemp+1,rgbtemp+2,&ticki->width);
          if(rgbtemp[0]<0.0||rgbtemp[0]>1.0||
             rgbtemp[1]<0.0||rgbtemp[1]>1.0||
             rgbtemp[2]<0.0||rgbtemp[2]>1.0){
            ticki->useforegroundcolor=1;
          }
          else{
            ticki->useforegroundcolor=0;
          }
          if(ticki->width<0.0)ticki->width=1.0;
        }
        for(i=0;i<3;i++){
          term = endt[i]-begt[i];
          length += term*term;
        }
        if(length<=0.0){
          endt[0]=begt[0]+1.0;
          length = 1.0;
        }
        ticki->length=sqrt(length);
        dxyz[0] =  0.0;
        dxyz[1] =  0.0;
        dxyz[2] =  0.0;
        switch (ticki->dir){
        case 1:
        case -1:
          dxyz[0]=1.0;
          break;
        case 2:
        case -2:
          dxyz[1]=1.0;
          break;
        case 3:
        case -3:
          dxyz[2]=1.0;
          break;
        default:
          ASSERT(FFALSE);
          break;
        }
        if(ticki->dir<0){
          for(i=0;i<3;i++){
            dxyz[i]=-dxyz[i];
          }
        }
        sum = 0.0;
        sum = dxyz[0]*dxyz[0] + dxyz[1]*dxyz[1] + dxyz[2]*dxyz[2];
        if(sum>0.0){
          sum=sqrt(sum);
          dxyz[0] *= (ticki->dlength/sum);
          dxyz[1] *= (ticki->dlength/sum);
          dxyz[2] *= (ticki->dlength/sum);
        }
      }
      continue;
    }

      if(localfile==1){
        tours_flag=0;
        if(match(buffer,"TOURS")==1)tours_flag=1;
        if( tours_flag==1){
          if(ntours>0){
            for(i=0;i<ntours;i++){
              tourdata *touri;

              touri = tourinfo + i;
              freetour(touri);
            }
            FREEMEMORY(tourinfo);
          }
          ntours=0;

          fgets(buffer,255,stream);
          sscanf(buffer,"%i",&ntours);
          ntours++;
          if(ntours>0){
            if(NewMemory( (void **)&tourinfo, ntours*sizeof(tourdata))==0)return 2;
            for(i=0;i<ntours;i++){
              tourdata *touri;

              touri=tourinfo+i;
              touri->path_times=NULL;
              touri->pathnodes=NULL;
            }
          }
          ReallocTourMemory();
          init_circulartour();
          {
            keyframe *thisframe, *addedframe;
            tourdata *touri;
            int glui_avatar_index_local;

            for(i=1;i<ntours;i++){
              touri = tourinfo + i;
              inittour(touri);
              fgets(buffer,255,stream);
              trim(buffer);
              strcpy(touri->label,buffer);

              t_globaltension=touri->global_tension;
              t_globaltension_flag=touri->global_tension_flag;
              fgets(buffer,255,stream);
              glui_avatar_index_local=0;
              sscanf(buffer,"%i %i %f %i %i",
                &nkeyframes,&t_globaltension_flag,&t_globaltension,&glui_avatar_index_local,&touri->display2);
              glui_avatar_index_local=CLAMP(glui_avatar_index_local,0,navatar_types-1);
              touri->glui_avatar_index=glui_avatar_index_local;
              if(touri->display2!=1)touri->display2=0;
              touri->global_tension_flag=t_globaltension_flag;
              touri->global_tension=t_globaltension;
              touri->nkeyframes=nkeyframes;

              if(NewMemory( (void **)&touri->keyframe_times, nkeyframes*sizeof(float))==0)return 2;
              if(NewMemory((void **)&touri->pathnodes,view_ntimes*sizeof(pathdata))==0)return 2;
              if(NewMemory((void **)&touri->path_times,view_ntimes*sizeof(float))==0)return 2;

              thisframe=&touri->first_frame;
              for(j=0;j<nkeyframes;j++){
                params[0] = 0.0;
                params[1] = 0.0;
                params[2] = 0.0;
                key_view[0]=0.0;
                key_view[1]=0.0;
                key_view[2]=0.0;
                key_bank=0.0;
                key_az_path = 0.0;
                key_elev_path=0.0;
                viewtype=0;
                zzoom=1.0;
                uselocalspeed=0;
                fgets(buffer,255,stream);

                sscanf(buffer,"%f %f %f %f %i",
                  &key_time,
                  key_xyz,key_xyz+1,key_xyz+2,
                  &viewtype);

                if(viewtype==0){
                  sscanf(buffer,"%f %f %f %f %i %f %f %f %f %f %f %f %i",
                  &key_time,
                  key_xyz,key_xyz+1,key_xyz+2,
                  &viewtype, &key_az_path, &key_elev_path,&key_bank, 
                  params,params+1,params+2,
                  &zzoom,&uselocalspeed);
                }
                else{
                  sscanf(buffer,"%f %f %f %f %i %f %f %f %f %f %f %f %i",
                  &key_time,
                  key_xyz,key_xyz+1,key_xyz+2,
                  &viewtype, key_view, key_view+1, key_view+2,
                  params,params+1,params+2,
                  &zzoom,&uselocalspeed);
                }
                if(zzoom<0.25)zzoom=0.25;
                if(zzoom>4.00)zzoom=4.0;
                addedframe=add_frame(thisframe, key_time, key_xyz, key_az_path, key_elev_path, 
                  key_bank, params, viewtype,zzoom,key_view);
                thisframe=addedframe;
                touri->keyframe_times[j]=key_time;
              }
            }
          }
          if(tours_flag==1){
            for(i=0;i<ntours;i++){
              tourdata *touri;

              touri=tourinfo+i;
              touri->first_frame.next->prev=&touri->first_frame;
              touri->last_frame.prev->next=&touri->last_frame;
            }
            updatetourmenulabels();
            createtourpaths();
            updatetimes();
            plotstate=getplotstate(DYNAMIC_PLOTS);
            selectedtour_index=-1;
            selected_frame=NULL;
            selected_tour=NULL;
            if(viewalltours==1)TourMenu(-3);
          }
          else{
            ntours=0;
          }
          strcpy(buffer,"1.00000 1.00000 2.0000 0");
          trimmzeros(buffer);
          continue;
        }
        if(match(buffer,"TOURINDEX")){
          if(fgets(buffer,255,stream)==NULL)break;
          sscanf(buffer,"%i",&selectedtour_index_ini);
          if(selectedtour_index_ini<0)selectedtour_index_ini=-1;
          update_selectedtour_index=1;
        }
      }
    }

  } 
  fclose(stream);
  return 0;

}

/* ------------------ writeini ------------------------ */

void writeini(int flag){
  char buffer[1024];

  float *iso_colors_temp;
  int n_iso_colors_temp;
  int n3d;
  int nn;
  extern int numplot3dvars;
  FILE *fileout;
  int i;
  int j;
  char *outfilename=NULL;

  fileout=NULL;
  switch (flag) {
  case GLOBAL_INI:
    fileout=fopen(INIfile,"w");
    outfilename=INIfile;
    break;
  case STDOUT_INI:
    fileout=stdout;
    break;
  case SCRIPT_INI:
    fileout=fopen(scriptinifilename,"w");
    outfilename=scriptinifilename;
    break;
  case LOCAL_INI:
    fileout=fopen(caseinifilename,"w");
    outfilename=caseinifilename;
    break;
  default:
    ASSERT(FFALSE);
    break;
  }
  if(flag==SCRIPT_INI)flag=LOCAL_INI;
  if(fileout==NULL){
    if(outfilename!=NULL){
      printf("error: unable to open %s for writing\n",outfilename);
      return;
    }
    else{
      printf("error: unable to open ini file for output\n");
    }
  }


  fprintf(fileout,"# NIST Smokeview configuration file, Release %s\n\n",__DATE__);
  fprintf(fileout,"COLORS\n");
  fprintf(fileout,"------\n\n");
  fprintf(fileout,"COLORBAR\n");
  fprintf(fileout," %i %i %i\n",nrgb,usetexturebar,colorbar_select_index);
  for(nn=0;nn<nrgb;nn++){
    fprintf(fileout," %f %f %f\n",rgb[nn][0],rgb[nn][1],rgb[nn][2]);
  }
  fprintf(fileout,"COLOR2BAR\n");
  fprintf(fileout," %i\n",8);
  fprintf(fileout," %f %f %f :white  \n",rgb2[0][0],rgb2[0][1],rgb2[0][2]);
  fprintf(fileout," %f %f %f :yellow \n",rgb2[1][0],rgb2[1][1],rgb2[1][2]);
  fprintf(fileout," %f %f %f :blue   \n",rgb2[2][0],rgb2[2][1],rgb2[2][2]);
  fprintf(fileout," %f %f %f :red    \n",rgb2[3][0],rgb2[3][1],rgb2[3][2]);
  fprintf(fileout," %f %f %f :green  \n",rgb2[4][0],rgb2[4][1],rgb2[4][2]);
  fprintf(fileout," %f %f %f :magenta\n",rgb2[5][0],rgb2[5][1],rgb2[5][2]);
  fprintf(fileout," %f %f %f :cyan   \n",rgb2[6][0],rgb2[6][1],rgb2[6][2]);
  fprintf(fileout," %f %f %f :black  \n",rgb2[7][0],rgb2[7][1],rgb2[7][2]);
  if(iso_ambient_ini!=NULL){
    n_iso_colors_temp=n_iso_ambient_ini;
    iso_colors_temp=iso_ambient_ini;
  }
  else{
    n_iso_colors_temp=n_iso_ambient;
    iso_colors_temp=iso_ambient;
  }
  fprintf(fileout,"ISOCOLORS\n");
	fprintf(fileout," %f %f : shininess, transparency\n",iso_shininess, iso_transparency);
	fprintf(fileout," %f %f %f : specular\n",iso_specular[0],iso_specular[1],iso_specular[2]);
  fprintf(fileout," %i\n",n_iso_colors_temp);
  for(nn=0;nn<n_iso_colors_temp;nn++){
    fprintf(fileout," %f %f %f\n",iso_colors_temp[4*nn],iso_colors_temp[4*nn+1],iso_colors_temp[4*nn+2]);
  }
  fprintf(fileout,"VENTCOLOR\n");
  fprintf(fileout," %f %f %f\n",ventcolor[0],ventcolor[1],ventcolor[2]);
  fprintf(fileout,"SENSORCOLOR\n");
  fprintf(fileout," %f %f %f\n",sensorcolor[0],sensorcolor[1],sensorcolor[2]);
  fprintf(fileout,"SENSORNORMCOLOR\n");
  fprintf(fileout," %f %f %f\n",sensornormcolor[0],sensornormcolor[1],sensornormcolor[2]);
  fprintf(fileout,"HEATONCOLOR\n");
  fprintf(fileout," %f %f %f\n",heatoncolor[0],heatoncolor[1],heatoncolor[2]);
  fprintf(fileout,"HEATOFFCOLOR\n");
  fprintf(fileout," %f %f %f\n",heatoffcolor[0],heatoffcolor[1],heatoffcolor[2]);
  fprintf(fileout,"SPRINKONCOLOR\n");
  fprintf(fileout," %f %f %f\n",sprinkoncolor[0],sprinkoncolor[1],sprinkoncolor[2]);
  fprintf(fileout,"SPRINKOFFCOLOR\n");
  fprintf(fileout," %f %f %f\n",sprinkoffcolor[0],sprinkoffcolor[1],sprinkoffcolor[2]);
  fprintf(fileout,"BLOCKCOLOR\n");
  fprintf(fileout," %f %f %f\n",block_ambient2[0],block_ambient2[1],block_ambient2[2]);
  fprintf(fileout,"BOUNDCOLOR\n");
  fprintf(fileout," %f %f %f\n",boundcolor[0],boundcolor[1],boundcolor[2]);
  fprintf(fileout,"STATICPARTCOLOR\n");
  fprintf(fileout," %f %f %f\n",static_color[0],static_color[1],static_color[2]);
  fprintf(fileout,"BACKGROUNDCOLOR\n");
  fprintf(fileout," %f %f %f\n",backgroundbasecolor[0],backgroundbasecolor[1],backgroundbasecolor[2]);
  fprintf(fileout,"FOREGROUNDCOLOR\n");
  fprintf(fileout," %f %f %f\n",foregroundbasecolor[0],foregroundbasecolor[1],foregroundbasecolor[2]);
/*  extern GLfloat iso_ambient[4], iso_specular[4], iso_shininess;*/

  fprintf(fileout,"FLIP\n");
  fprintf(fileout," %i\n",background_flip);
  fprintf(fileout,"TIMEBARCOLOR\n");
  fprintf(fileout," %f %f %f\n",timebarcolor[0],timebarcolor[1],timebarcolor[2]);
  fprintf(fileout,"SETBW\n");
  fprintf(fileout," %i\n",setbw);
  fprintf(fileout,"COLORBARFLIP\n");
  fprintf(fileout," %i\n",colorbarflip);

  fprintf(fileout,"\n LIGHTING\n");
  fprintf(fileout,"--------\n\n");
#ifdef pp_SHOWLIGHT
  fprintf(fileout,"SHOWLIGHT0\n");
  fprintf(fileout," %i\n",visLIGHT0);
  fprintf(fileout,"SHOWLIGHT1\n");
  fprintf(fileout," %i\n",visLIGHT1);
  fprintf(fileout,"SHOWLIGHTMENU\n");
  fprintf(fileout," %i\n",showlightmenu);
#endif
  fprintf(fileout,"AMBIENTLIGHT\n");
  fprintf(fileout," %f %f %f\n",ambientlight[0],ambientlight[1],ambientlight[2]);
  fprintf(fileout,"DIFFUSELIGHT\n");
  fprintf(fileout," %f %f %f\n",diffuselight[0],diffuselight[1],diffuselight[2]);

  fprintf(fileout,"\n SIZES\n");
  fprintf(fileout,"-----------\n\n");
  fprintf(fileout,"VECTORPOINTSIZE\n");
  fprintf(fileout," %f\n",vectorpointsize);
  fprintf(fileout,"VECTORLINEWIDTH\n");
  fprintf(fileout," %f\n",vectorlinewidth);
  fprintf(fileout,"VECLENGTH\n");
  fprintf(fileout," %i\n",iveclengths);
  fprintf(fileout,"VECTORLENGTH\n");
  fprintf(fileout," %f\n",vecfactor);
  fprintf(fileout,"VECTORPOINTSIZE\n");
  fprintf(fileout," %f\n",vectorpointsize);
  fprintf(fileout,"PARTPOINTSIZE\n");
  fprintf(fileout," %f\n",partpointsize);
  fprintf(fileout,"STREAKLINEWIDTH\n");
  fprintf(fileout," %f\n",streaklinewidth);
  fprintf(fileout,"ISOPOINTSIZE\n");
  fprintf(fileout," %f\n",isopointsize);
  fprintf(fileout,"ISOLINEWIDTH\n");
  fprintf(fileout," %f\n",isolinewidth);
  fprintf(fileout,"PLOT3DPOINTSIZE\n");
  fprintf(fileout," %f\n",plot3dpointsize);
  fprintf(fileout,"PLOT3DLINEWIDTH\n");
  fprintf(fileout," %f\n",plot3dlinewidth);
  fprintf(fileout,"SENSORABSSIZE\n");
  fprintf(fileout," %f\n",sensorabssize);
  fprintf(fileout,"SENSORRELSIZE\n");
  fprintf(fileout," %f\n",sensorrelsize);
  fprintf(fileout,"SPRINKLERABSSIZE\n");
  fprintf(fileout," %f\n",sprinklerabssize);
  fprintf(fileout,"SPHERESEGS\n");
  fprintf(fileout," %i\n",device_sphere_segments);
  if(use_graphics==1&&
     (screenWidth == glutGet(GLUT_SCREEN_WIDTH)||screenHeight == glutGet(GLUT_SCREEN_HEIGHT))
    ){
    fprintf(fileout,"WINDOWWIDTH\n");
    fprintf(fileout," %i\n",-1);
    fprintf(fileout,"WINDOWHEIGHT\n");
    fprintf(fileout," %i\n",-1);
  }
  else{
    fprintf(fileout,"WINDOWWIDTH\n");
    fprintf(fileout," %i\n",screenWidth);
    fprintf(fileout,"WINDOWHEIGHT\n");
    fprintf(fileout," %i\n",screenHeight);
  }
  fprintf(fileout,"WINDOWOFFSET\n");
  fprintf(fileout," %i\n",titlesafe_offsetBASE);
  fprintf(fileout,"RENDEROPTION\n");
  fprintf(fileout," %i\n",render_option);
  fprintf(fileout,"\n LINES\n");
  fprintf(fileout,"-----------\n\n");
  fprintf(fileout,"LINEWIDTH\n");
  fprintf(fileout," %f\n",linewidth);
  fprintf(fileout,"VENTLINEWIDTH\n");
  fprintf(fileout," %f\n",ventlinewidth);
  fprintf(fileout,"SMOOTHLINES\n");
  fprintf(fileout," %i\n",antialiasflag);
  fprintf(fileout,"USENEWDRAWFACE\n");
  fprintf(fileout," %i\n",use_new_drawface);

  fprintf(fileout,"\nOFFSETS\n");
  fprintf(fileout,"-------\n\n");
  fprintf(fileout,"VENTOFFSET\n");
  fprintf(fileout," %f\n",ventoffset_factor);
  fprintf(fileout,"SLICEOFFSET\n");
  fprintf(fileout," %f\n",sliceoffset_factor);

  fprintf(fileout,"\nTIME MIN/MAX\n");
  fprintf(fileout,"------------\n");
  fprintf(fileout,"(0/1 min max skip (1=set, 0=unset)\n\n");
  fprintf(fileout,"TLOAD\n");
  fprintf(fileout," %i %f %i %f %i %i\n",use_tload_begin,tload_begin,use_tload_end,tload_end,use_tload_skip,tload_skip);

  fprintf(fileout,"\nVALUE MIN/MAX\n");
  fprintf(fileout,"-------------\n");
  fprintf(fileout,"(0/1 min 0/1 max (1=set, 0=unset)\n\n");
  if(npart5prop>0){
    for(i=0;i<npart5prop;i++){
      part5prop *propi;

      propi = part5propinfo + i;
      fprintf(fileout,"V5_PARTICLES\n");
      fprintf(fileout," %i %f %i %f %s\n",
        propi->setvalmin,propi->valmin,propi->setvalmax,propi->valmax,propi->label->shortlabel);
    }
  }
  fprintf(fileout,"V_PARTICLES\n");
  fprintf(fileout," %i %f %i %f\n",setpartmin,partmin,setpartmax,partmax);
  fprintf(fileout,"C_PARTICLES\n");
  fprintf(fileout," %i %f %i %f\n",setpartchopmin,partchopmin,setpartchopmax,partchopmax);
  if(nslice2>0){
    for(i=0;i<nslice2;i++){
      fprintf(fileout,"V_SLICE\n");
#ifdef pp_SLICECONTOURS
      fprintf(fileout," %i %f %i %f %s : %f %f %i\n",
#else
      fprintf(fileout," %i %f %i %f %s\n",
#endif
        slicebounds[i].setvalmin,slicebounds[i].valmin,
        slicebounds[i].setvalmax,slicebounds[i].valmax,
        slicebounds[i].label->shortlabel
#ifdef pp_SLICECONTOURS
        ,slicebounds[i].line_contour_min,slicebounds[i].line_contour_max,slicebounds[i].line_contour_num
#endif
        );
    }
    for(i=0;i<nslice2;i++){
      fprintf(fileout,"C_SLICE\n");
      fprintf(fileout," %i %f %i %f %s\n",
        slicebounds[i].setchopmin,slicebounds[i].chopmin,
        slicebounds[i].setchopmax,slicebounds[i].chopmax,
        slicebounds[i].label->shortlabel
        );
    }
    fprintf(fileout,"SLICEDATAOUT\n");
    fprintf(fileout," %i \n",output_slicedata);
  }
  if(niso_bounds>0){
    for(i=0;i<niso_bounds;i++){
      fprintf(fileout,"V_ISO\n");
      fprintf(fileout," %i %f %i %f %s\n",
        isobounds[i].setvalmin,isobounds[i].valmin,
        isobounds[i].setvalmax,isobounds[i].valmax,
        isobounds[i].label->shortlabel
        );
    }
    for(i=0;i<niso_bounds;i++){
      fprintf(fileout,"C_ISO\n");
      fprintf(fileout," %i %f %i %f %s\n",
        isobounds[i].setchopmin,isobounds[i].chopmin,
        isobounds[i].setchopmax,isobounds[i].chopmax,
        isobounds[i].label->shortlabel
        );
    }
  }
  for(i=0;i<npatchinfo;i++){
    if(patchinfo[i].firstshort==1){
      fprintf(fileout,"V_BOUNDARY\n");
      fprintf(fileout," %i %f %i %f %s\n",
        patchinfo[i].setvalmin,patchinfo[i].valmin,
        patchinfo[i].setvalmax,patchinfo[i].valmax,
        patchinfo[i].label.shortlabel
        );
    }
  }
  for(i=0;i<npatch2;i++){
    int ii;
    patchdata *patchi;

    ii = patchlabellist_index[i];
    patchi = patchinfo + ii;
    fprintf(fileout,"C_BOUNDARY\n");
    fprintf(fileout," %i %f %i %f %s\n",
      patchi->setchopmin,patchi->chopmin,
      patchi->setchopmax,patchi->chopmax,
      patchi->label.shortlabel
      );
  }
  fprintf(fileout,"V_ZONE\n");
  fprintf(fileout," %i %f %i %f\n",setzonemin,zonemin,setzonemax,zonemax);

  fprintf(fileout,"V_PLOT3D\n");
  n3d = 5;
  if(n3d<numplot3dvars)n3d=numplot3dvars;
  if(n3d>mxplot3dvars)n3d=mxplot3dvars;
  fprintf(fileout," %i\n",n3d);
  for(i=0;i<n3d;i++){
    fprintf(fileout," %i %i %f %i %f\n",i+1,setp3min[i],p3min[i],setp3max[i],p3max[i]);
  }
  fprintf(fileout,"C_PLOT3D\n");
  n3d = 5;
  if(n3d<numplot3dvars)n3d=numplot3dvars;
  if(n3d>mxplot3dvars)n3d=mxplot3dvars;
  fprintf(fileout," %i\n",n3d);
  for(i=0;i<n3d;i++){
    fprintf(fileout," %i %i %f %i %f\n",i+1,setp3chopmin[i],p3chopmin[i],setp3chopmax[i],p3chopmax[i]);
  }

  fprintf(fileout,"UNLOAD_QDATA\n");
  fprintf(fileout," %i\n",unload_qdata);

  fprintf(fileout,"V_TARGET\n");
  fprintf(fileout," %i %f %i %f\n",settargetmin,targetmin,settargetmax,targetmax);
  fprintf(fileout,"PERCENTILELEVEL\n");
  fprintf(fileout," %f\n",percentile_level);


  fprintf(fileout,"\nDATA LOADING\n");
  fprintf(fileout,"------------\n\n");
  fprintf(fileout,"NOPART\n");
  fprintf(fileout," %i\n",nopart);
  fprintf(fileout,"PARTPOINTSTEP\n");
  fprintf(fileout," %i\n",partpointstep);
  fprintf(fileout,"SLICEAVERAGE\n");
  fprintf(fileout," %i %f %i\n",slice_average_flag,slice_average_interval,vis_slice_average);
  fprintf(fileout,"SMOKE3DZIPSTEP\n");
  fprintf(fileout," %i\n",smoke3dzipstep);
  fprintf(fileout,"ISOZIPSTEP\n");
  fprintf(fileout," %i\n",isozipstep);
  fprintf(fileout,"SLICEZIPSTEP\n");
  fprintf(fileout," %i\n",slicezipstep);
  fprintf(fileout,"BOUNDZIPSTEP\n");
  fprintf(fileout," %i\n",boundzipstep);
  fprintf(fileout,"SHOWTRACERSALWAYS\n");
  fprintf(fileout," %i\n",show_tracers_always);
  fprintf(fileout,"SHOWEVACSLICES\n");
  fprintf(fileout," %i %i %i\n",show_evac_slices,constant_evac_coloring,show_evac_colorbar);
  fprintf(fileout,"DIRECTIONCOLOR\n");
  fprintf(fileout," %f %f %f\n",direction_color[0],direction_color[1],direction_color[2]);
 
  if(flag==LOCAL_INI){
    fprintf(fileout,"AVATAREVAC\n");
    fprintf(fileout," %i\n",iavatar_evac);
  }
 
  if(flag==LOCAL_INI){
    {
      int ndevice_vis=0;
      sv_object *obj_typei;

      for(i=0;i<nobject_defs;i++){
        obj_typei = object_defs[i];
        if(obj_typei->used==1&&obj_typei->visible==1){
          ndevice_vis++;
        }
      }
      fprintf(fileout,"SHOWDEVICES\n");
      fprintf(fileout," %i\n",ndevice_vis);
      for(i=0;i<nobject_defs;i++){
        obj_typei = object_defs[i];
        if(obj_typei->used==1&&obj_typei->visible==1){
          fprintf(fileout," %s\n",obj_typei->label);
        }
      }
    }

    fprintf(fileout,"GRIDPARMS\n");
    fprintf(fileout,"%i %i %i\n",visx_all, visy_all, visz_all);
    fprintf(fileout,"%i %i %i\n",iplotx_all, iploty_all, iplotz_all);
    fprintf(fileout,"SHOWDEVICEVALS\n");
    fprintf(fileout," %i %i %i\n",showdeviceval,showvdeviceval,devicetypes_index);
    put_startup_smoke3d(fileout);
    fprintf(fileout,"LOADFILESATSTARTUP\n");
    fprintf(fileout," %i\n",loadfiles_at_startup);
    if(npart5prop>0){
      fprintf(fileout,"PART5PROPDISP\n");
      for(i=0;i<npart5prop;i++){
        part5prop *propi;

        propi = part5propinfo + i;
        fprintf(fileout," ");
        for(j=0;j<npartclassinfo;j++){
          fprintf(fileout," %i ",propi->class_vis[j]);
        }
        fprintf(fileout,"\n");
      }
      fprintf(fileout,"PART5COLOR\n");
      for(i=0;i<npart5prop;i++){
        part5prop *propi;

        propi = part5propinfo + i;
        if(propi->display==1){
          fprintf(fileout," %i\n",i);
          break;
        }
      }

    }
  }
  if(flag==LOCAL_INI&&npartclassinfo>0){
    fprintf(fileout,"PART5CLASSVIS\n");
    fprintf(fileout," %i\n",npartclassinfo);
    for(j=0;j<npartclassinfo;j++){
      part5class *partclassj;

      partclassj = partclassinfo + j;
      fprintf(fileout," %i\n",partclassj->vis_type);
    }
  }
  if(flag==LOCAL_INI&&npropinfo>0){
    fprintf(fileout,"PROPINDEX\n");
    fprintf(fileout," %i\n",npropinfo);
    for(i=0;i<npropinfo;i++){
      propdata *propi;
      int offset;
      int jj;

      propi = propinfo + i;
      offset=-1;
      for(jj=0;jj<propi->nsmokeview_ids;jj++){
        if(strcmp(propi->smokeview_id,propi->smokeview_ids[jj])==0){
          offset=jj;
          break;
        }
      }
      fprintf(fileout," %i %i\n",i,offset);
    }
  }

  fprintf(fileout,"\nCONTOURS\n");
  fprintf(fileout,"--------\n\n");
  fprintf(fileout,"CONTOURTYPE\n");
  fprintf(fileout," %i\n",contour_type);
  fprintf(fileout,"P3VIEW\n");
  for(i=0;i<nmeshes;i++){
    mesh *meshi;

    meshi = meshinfo + i;
    fprintf(fileout," %i %i %i %i %i %i \n",visx_all,meshi->plotx,visy_all,meshi->ploty,visz_all,meshi->plotz);
  }
  for(i=0;i<nmeshes;i++){
    mesh *meshi;

    meshi = meshinfo + i;
    if(meshi->mesh_offset_ptr!=NULL){
      fprintf(fileout,"MESHOFFSET\n");
      fprintf(fileout," %i\n",i);
    }
  }
  fprintf(fileout,"TRANSPARENT\n");
  fprintf(fileout," %i %f\n",use_transparency_data,transparent_level);
  fprintf(fileout,"SURFINC\n");
  fprintf(fileout," %i\n",surfincrement);
  fprintf(fileout,"P3DSURFACETYPE\n");
  fprintf(fileout," %i\n",p3dsurfacetype);
  fprintf(fileout,"P3DSURFACESMOOTH\n");
  fprintf(fileout," %i\n",p3dsurfacesmooth);

  fprintf(fileout,"\nVISIBILITY\n");
  fprintf(fileout,"----------\n\n");
  if(nmeshes>1){
    fprintf(fileout,"MESHVIS\n");
    fprintf(fileout," %i\n",nmeshes);

    for(i=0;i<nmeshes;i++){
      mesh *meshi;

      meshi = meshinfo + i;
      fprintf(fileout," %i\n",meshi->blockvis);
    }
  }
  fprintf(fileout,"SHOWTITLE\n");
  fprintf(fileout," %i\n",visTitle0);
  fprintf(fileout,"SHOWCOLORBARS\n");
  fprintf(fileout," %i\n",visColorbarLabels);
  fprintf(fileout,"SHOWBLOCKS\n");
  fprintf(fileout," %i\n",visBlocks);
  fprintf(fileout,"SHOWNORMALWHENSMOOTH\n");
  fprintf(fileout," %i\n",visSmoothAsNormal);
  fprintf(fileout,"SMOOTHBLOCKSOLID\n");
  fprintf(fileout," %i\n",smooth_block_solid);
  fprintf(fileout,"SBATSTART\n");
  fprintf(fileout," %i\n",sb_atstart);
  fprintf(fileout,"SHOWTRANSPARENT\n");
  fprintf(fileout," %i\n",visTransparentBlockage);
  fprintf(fileout,"SHOWVENTS\n");
  fprintf(fileout," %i %i %i\n",visVents,visVentLines,visVentSolid);
  fprintf(fileout,"SHOWTRANSPARENTVENTS\n");
  fprintf(fileout," %i\n",show_transparent_vents);
  fprintf(fileout,"SHOWSENSORS\n");
  fprintf(fileout," %i %i\n",visSensor,visSensorNorm);
  fprintf(fileout,"SHOWTIMEBAR\n");
  fprintf(fileout," %i\n",visTimeLabels);
  fprintf(fileout,"SHOWTIMELABEL\n");
  fprintf(fileout," %i\n",visTimelabel);
  fprintf(fileout,"SHOWFRAMELABEL\n");
  fprintf(fileout," %i\n",visFramelabel);
  fprintf(fileout,"SHOWFRAMELABEL\n");
  fprintf(fileout," %i\n",visFramelabel);
  fprintf(fileout,"SHOWFLOOR\n");
  fprintf(fileout," %i\n",visFloor);
  fprintf(fileout,"SHOWWALLS\n");
  fprintf(fileout," %i\n",visWalls);
  fprintf(fileout,"SHOWCEILING\n");
  fprintf(fileout," %i\n",visCeiling);
  fprintf(fileout,"SHOWSMOKEPART\n");
  fprintf(fileout," %i\n",visSmokePart);
  fprintf(fileout,"SHOWSPRINKPART\n");
  fprintf(fileout," %i\n",visSprinkPart);
#ifdef pp_memstatus
  fprintf(fileout,"SHOWMEMLOAD\n");
  fprintf(fileout," %i\n",visAvailmemory);
#endif
  fprintf(fileout,"SHOWBLOCKLABEL\n");
  fprintf(fileout," %i\n",visBlocklabel);
  fprintf(fileout,"SHOWAXISLABELS\n");
  fprintf(fileout," %i\n",visaxislabels);
  fprintf(fileout,"SHOWFRAME\n");
  fprintf(fileout," %i\n",visFrame);
  fprintf(fileout,"SHOWALLTEXTURES\n");
  fprintf(fileout," %i\n",showall_textures);
  fprintf(fileout,"SHOWTHRESHOLD\n");
  fprintf(fileout," %i %i %f\n",vis_threshold,vis_onlythreshold,temp_threshold);
  fprintf(fileout,"SHOWHRRCUTOFF\n");
  fprintf(fileout," %i\n",show_hrrcutoff);
  fprintf(fileout,"TWOSIDEDVENTS\n");
  fprintf(fileout," %i %i\n",show_bothsides_int,show_bothsides_ext);
  fprintf(fileout,"TRAINERVIEW\n");
  fprintf(fileout," %i\n",trainerview);
  fprintf(fileout,"SHOWTERRAIN\n");
  fprintf(fileout," %i\n",visTerrainType);
  fprintf(fileout,"TERRAINPARMS\n");
  fprintf(fileout," %i %i %i\n",terrain_rgba_zmin[0],terrain_rgba_zmin[1],terrain_rgba_zmin[2]);
  fprintf(fileout," %i %i %i\n",terrain_rgba_zmax[0],terrain_rgba_zmax[1],terrain_rgba_zmax[2]);
  fprintf(fileout," %f\n",vertical_factor);
  fprintf(fileout,"OFFSETSLICE\n");
  fprintf(fileout," %i\n",offset_slice);
  fprintf(fileout,"SHOWTRIANGLES\n");
  fprintf(fileout," %i %i %i %i %i %i\n",showtrisurface,showtrioutline,showtripoints,showtrinormal,showpointnormal,smoothtrinormal);
  fprintf(fileout,"SHOWSTREAK\n");
  fprintf(fileout," %i %i %i %i\n",streak5show,streak5step,showstreakhead,streak_index);
  fprintf(fileout,"ISOTRAN2\n");
  fprintf(fileout," %i\n",transparent_state);
  fprintf(fileout,"SHOWISO\n");
  fprintf(fileout," %i\n",visAIso);
  fprintf(fileout,"SHOWISONORMALS\n");
  fprintf(fileout," %i\n",showtrinormal);
  fprintf(fileout,"SMOKESENSORS\n");
  fprintf(fileout," %i %i\n",show_smokesensors,test_smokesensors);
  fprintf(fileout,"VOLSMOKE\n");
  fprintf(fileout," %i %i %i %i %f %f %f",
        glui_compress_volsmoke,use_multi_threading,load_at_rendertimes,volbw,
        temperature_cutoff,opacity_factor,mass_extinct
        );

  fprintf(fileout,"\nMISC\n");
  fprintf(fileout,"----\n\n");
  if(use_nistlogo==1){
    fprintf(fileout,"USENISTLOGO\n");
    fprintf(fileout," %i\n",use_nistlogo);
  }
  if(trainer_mode==1){
    fprintf(fileout,"TRAINERMODE\n");
    fprintf(fileout," %i\n",trainer_mode);
  }
#ifdef pp_LANG
  fprintf(fileout,"STARTUPLANG\n");
  fprintf(fileout," %s\n",startup_lang_code);
#endif
  fprintf(fileout,"SHOWOPENVENTS\n");
  fprintf(fileout," %i %i\n",visOpenVents,visOpenVentsAsOutline);
  fprintf(fileout,"SHOWDUMMYVENTS\n");
  fprintf(fileout," %i\n",visDummyVents);
  fprintf(fileout,"SHOWOTHERVENTS\n");
  fprintf(fileout," %i\n",visOtherVents);
  fprintf(fileout,"SHOWSLICEINOBST\n");
  fprintf(fileout," %i\n",show_slice_in_obst);
  fprintf(fileout,"SKIPEMBEDSLICE\n");
  fprintf(fileout," %i\n",skip_slice_in_embedded_mesh);
  fprintf(fileout,"CELLCENTERINTERP\n");
  fprintf(fileout," %i\n",cellcenter_interp);
  fprintf(fileout,"SHOWTICKS\n");
  fprintf(fileout," %i\n",visTicks);
  if(flag==LOCAL_INI){
    fprintf(fileout,"USERTICKS\n");
    fprintf(fileout," %i %i %i %i %i %i\n",vis_user_ticks,auto_user_tick_placement,user_tick_sub,
      user_tick_show_x,user_tick_show_y,user_tick_show_z);
    fprintf(fileout," %f %f %f\n",user_tick_origin[0],user_tick_origin[1],user_tick_origin[2]);
    fprintf(fileout," %f %f %f\n",user_tick_min[0],user_tick_min[1],user_tick_min[2]);
    fprintf(fileout," %f %f %f\n",user_tick_max[0],user_tick_max[1],user_tick_max[2]);
    fprintf(fileout," %f %f %f\n",user_tick_step[0],user_tick_step[1],user_tick_step[2]);
    fprintf(fileout," %i %i %i\n",user_tick_show_x,user_tick_show_y,user_tick_show_z);
  }
#ifdef pp_SHOOTER
  if(flag==LOCAL_INI){
    fprintf(fileout,"SHOOTER\n");
    fprintf(fileout," %f %f %f\n",shooter_xyz[0],shooter_xyz[1],shooter_xyz[2]);
    fprintf(fileout," %f %f %f\n",shooter_dxyz[0],shooter_dxyz[1],shooter_dxyz[2]);
    fprintf(fileout," %f %f %f\n",shooter_uvw[0],shooter_uvw[1],shooter_uvw[2]);
    fprintf(fileout," %f %f %f\n",   shooter_velmag, shooter_veldir, shooterpointsize);
    fprintf(fileout," %i %i %i %i %i\n",shooter_fps,shooter_vel_type,shooter_nparts,visShooter,shooter_cont_update);
    fprintf(fileout," %f %f\n",shooter_duration,shooter_v_inf);
  }
#endif
  fprintf(fileout,"SHOWLABELS\n");
  fprintf(fileout," %i\n",visLabels);
  fprintf(fileout,"SHOWFRAMERATE\n");
  fprintf(fileout," %i\n",visFramerate);
  fprintf(fileout,"FRAMERATEVALUE\n");
  fprintf(fileout," %i\n",frameratevalue);
  fprintf(fileout,"VECTORSKIP\n");
  fprintf(fileout," %i\n",vectorskip);
  fprintf(fileout,"AXISSMOOTH\n");
  fprintf(fileout," %i\n",axislabels_smooth);
  fprintf(fileout,"BLOCKLOCATION\n");
  fprintf(fileout," %i\n",blocklocation);
  fprintf(fileout,"SHOWCADANDGRID\n");
  fprintf(fileout," %i\n",show_cad_and_grid);
  fprintf(fileout,"OUTLINEMODE\n");
  fprintf(fileout," %i\n",highlight_flag);
  fprintf(fileout,"TITLESAFE\n");
  fprintf(fileout," %i\n",titlesafe_offset);
  fprintf(fileout,"FONTSIZE\n");
  fprintf(fileout," %i\n",fontindex);
  fprintf(fileout,"ZOOM\n");
  fprintf(fileout," %i %f\n",zoomindex,zoom);
  fprintf(fileout,"APERTURE\n");
  fprintf(fileout," %i\n",apertureindex);
  fprintf(fileout,"RENDERFILETYPE\n");
  fprintf(fileout," %i\n",renderfiletype);
  fprintf(fileout,"RENDERFILELABEL\n");
  fprintf(fileout," %i\n",renderfilelabel);
  fprintf(fileout,"SHOWGRIDLOC\n");
  fprintf(fileout," %i\n",visgridloc);
  fprintf(fileout,"PIXELSKIP\n");
  fprintf(fileout," %i\n",pixel_skip);
  fprintf(fileout,"PROJECTION\n");
  fprintf(fileout," %i\n",projection_type);
  fprintf(fileout,"STEREO\n");
  fprintf(fileout," %i\n",showstereo);

  if(nskyboxinfo>0){
    int iskybox;
    skyboxdata *skyi;
    char *filei;
    char *nullfile="NULL";

    for(iskybox=0;iskybox<nskyboxinfo;iskybox++){
      skyi = skyboxinfo + iskybox;
      fprintf(fileout,"SKYBOX\n");
      for(i=0;i<6;i++){
        filei = skyi->face[i].file;
        if(filei==NULL)filei=nullfile;
        if(strcmp(filei,"NULL")==0){
          fprintf(fileout,"NULL\n");
        }
        else{
          fprintf(fileout," %s\n",filei);
        }
      }
    }
  }

  fprintf(fileout,"UNITCLASSES\n");
  fprintf(fileout," %i\n",nunitclasses);
  for(i=0;i<nunitclasses;i++){
    fprintf(fileout," %i\n",unitclasses[i].active);
  }
  if(flag==LOCAL_INI){
    fprintf(fileout,"MSCALE\n");
    fprintf(fileout," %f %f %f\n",mscale[0],mscale[1],mscale[2]);
  }
  fprintf(fileout,"CLIP\n");
  fprintf(fileout," %f %f\n",nearclip,farclip);

  if(flag==LOCAL_INI){
    fprintf(fileout,"XYZCLIP\n");
    fprintf(fileout," %i\n",xyz_clipplane);
    fprintf(fileout," %i %f %i %f\n",clip_x, clip_x_val, clip_X, clip_X_val);
    fprintf(fileout," %i %f %i %f\n",clip_y, clip_y_val, clip_Y, clip_Y_val);
    fprintf(fileout," %i %f %i %f\n",clip_z, clip_z_val, clip_Z, clip_Z_val);

    for(i=nlabelssmv;i<nlabels;i++){
      labeldata *labeli;
      float *xyz, *rgbtemp, *tstart_stop;

      labeli = labelinfo + i;
      xyz = labeli->xyz;
      rgbtemp = labeli->rgb;
      tstart_stop = labeli->tstart_stop;

      fprintf(fileout,"LABEL\n");
      fprintf(fileout," %f %f %f %f %f %f %f %f\n",xyz[0],xyz[1],xyz[2],rgbtemp[0],rgbtemp[1],rgbtemp[2],tstart_stop[0],tstart_stop[1]);
      fprintf(fileout," %s\n",labeli->label);
    }

    
    for(i=ntickssmv;i<nticks;i++){
      float *begt;
      float *endt;
      float *rgbtemp;
      tickdata *ticki;

      ticki = tickinfo + i;
      begt = ticki->begin;
      endt = ticki->end;
      rgbtemp = ticki->rgb;

      fprintf(fileout,"TICKS\n");
      fprintf(fileout," %f %f %f %f %f %f %i\n",begt[0],begt[1],begt[2],endt[0],endt[1],endt[2],ticki->nbars);
      fprintf(fileout," %f %i %f %f %f %f\n",ticki->dlength,ticki->dir,rgbtemp[0],rgbtemp[1],rgbtemp[2],ticki->width);
    }
    
  }
  if(fds_filein!=NULL&&strlen(fds_filein)>0){
    fprintf(fileout,"INPUT_FILE\n");
    fprintf(fileout," %s\n",fds_filein);
  }

  fprintf(fileout,"EYEX\n");
  fprintf(fileout," %f\n",eyexfactor);
  fprintf(fileout,"EYEY\n");
  fprintf(fileout," %f\n",eyeyfactor);
  fprintf(fileout,"EYEZ\n");
  fprintf(fileout," %f\n",eyezfactor);
  fprintf(fileout,"EYEVIEW\n");
  fprintf(fileout," %i\n",eyeview);
  {
    char *label;

    label = get_camera_label(startup_view_ini);
    if(label!=NULL){
      fprintf(fileout,"LABELSTARTUPVIEW\n");
      fprintf(fileout," %s\n",label);
    }
  }
  fprintf(fileout,"VIEWTIMES\n");
  fprintf(fileout," %f %f %i\n",view_tstart,view_tstop,view_ntimes);
  fprintf(fileout,"TIMEOFFSET\n");
  fprintf(fileout," %f\n",timeoffset);
  fprintf(fileout,"SHOWHMSTIMELABEL\n");
  fprintf(fileout," %i\n",vishmsTimelabel);
  fprintf(fileout,"SPEED\n");
  fprintf(fileout," %f %f\n",speed_crawl,speed_walk);

  fprintf(fileout,"CULLFACES\n");
  fprintf(fileout," %i\n",cullfaces);
  fprintf(fileout,"\nZone\n");
  fprintf(fileout,"----\n\n");
  fprintf(fileout,"SHOWZONEFIRE\n");
  fprintf(fileout," %i\n",viszonefire);
  fprintf(fileout,"SHOWSZONE\n");
  fprintf(fileout," %i\n",visSZone);
  fprintf(fileout,"SHOWHZONE\n");
  fprintf(fileout," %i\n",visHZone);
  fprintf(fileout,"SHOWVZONE\n");
  fprintf(fileout," %i\n",visVZone);
  fprintf(fileout,"SHOWHAZARDCOLORS\n");
  fprintf(fileout," %i\n",sethazardcolor);
  if(
    ((INI_fds_filein!=NULL&&fds_filein!=NULL&&strcmp(INI_fds_filein,fds_filein)==0)||
    flag==LOCAL_INI)){
    {
      float *eye, *angle_zx, *mat;
      camera *ca;

      for(ca=camera_list_first.next;ca->next!=NULL;ca=ca->next){
        if(strcmp(ca->name,"internal")==0)continue;
        if(strcmp(ca->name,"external")==0)continue;
        fprintf(fileout,"VIEWPOINT5\n");
        eye = ca->eye;
        angle_zx = ca->angle_zx;
        mat = ca->modelview;

		    fprintf(fileout," %i %i %i\n",
          ca->eyeview,
          ca->rotation_index,
          ca->view_id);
		    fprintf(fileout," %f %f %f %f %i\n",
          eye[0],eye[1],eye[2],
          zoom,zoomindex);
  		  fprintf(fileout," %f %f %f %i\n",
          ca->view_angle, 
          ca->direction_angle,
          ca->elevation_angle,
          ca->projection_type);
		    fprintf(fileout," %f %f %f\n",
          ca->xcen,
          ca->ycen,
          ca->zcen);

        fprintf(fileout," %f %f\n",angle_zx[0],angle_zx[1]);
        fprintf(fileout," %f %f %f %f\n",mat[0],mat[1],mat[2],mat[3]);
        fprintf(fileout," %f %f %f %f\n",mat[4],mat[5],mat[6],mat[7]);
        fprintf(fileout," %f %f %f %f\n",mat[8],mat[9],mat[10],mat[11]);
        fprintf(fileout," %f %f %f %f\n",mat[12],mat[13],mat[14],mat[15]);
        fprintf(fileout," %i %i %i %i %i %i %i\n",
            ca->xyz_clipplane,
            ca->clip_x,ca->clip_y,ca->clip_z,
            ca->clip_X,ca->clip_Y,ca->clip_Z);
        fprintf(fileout," %f %f %f %f %f %f\n",
            ca->clip_x_val,ca->clip_y_val,ca->clip_z_val,
            ca->clip_X_val,ca->clip_Y_val,ca->clip_Z_val);
        fprintf(fileout," %s\n",ca->name);
      }
    }
  }

  fprintf(fileout,"\n3D SMOKE INFO\n");
  fprintf(fileout,"-------------\n\n");
  fprintf(fileout,"ADJUSTALPHA\n");
  fprintf(fileout," %i\n",adjustalphaflag);
#ifdef pp_GPU
  fprintf(fileout,"USEGPU\n");
  fprintf(fileout," %i\n",usegpu);
#endif
  fprintf(fileout,"SMOKECULL\n");
#ifdef pp_CULL
  fprintf(fileout," %i\n",cullsmoke);
#else
  fprintf(fileout," %i\n",smokecullflag);
#endif
  fprintf(fileout,"SMOKESKIP\n");
  fprintf(fileout," %i\n",smokeskipm1);
  fprintf(fileout,"SMOKEALBEDO\n");
  fprintf(fileout," %f\n",smoke_shade);
#ifdef pp_GPU
  fprintf(fileout,"SMOKERTHICK\n");
  fprintf(fileout," %f\n",smoke3d_rthick);
#else
  fprintf(fileout,"SMOKETHICK\n");
  fprintf(fileout," %i\n",smoke3d_thick);
#endif
  fprintf(fileout,"FIRECOLOR\n");
  fprintf(fileout," %i %i %i\n",fire_red,fire_green,fire_blue);
  fprintf(fileout,"FIREDEPTH\n");
  fprintf(fileout," %f\n",fire_halfdepth);

  if(colorbartype!=colorbartype_default){
    fprintf(fileout,"COLORBARTYPE\n");
    fprintf(fileout," %i\n",colorbartype);
  }
  fprintf(fileout,"VOLSMOKE\n");
  fprintf(fileout," %i %i %i %i %i\n",
    glui_compress_volsmoke,use_multi_threading,load_at_rendertimes,volbw,show_volsmoke_moving);
  fprintf(fileout," %f %f %f %f %f\n",
    temperature_min,temperature_cutoff,temperature_max,opacity_factor,mass_extinct);
  fprintf(fileout,"FIRECOLORMAP\n");
  fprintf(fileout," %i %i\n",use_firesmokemap,fire_colorbar_index);
  fprintf(fileout,"SHOWEXTREMEDATA\n");
  fprintf(fileout," %i\n",show_extremedata);
  {
    int mmin[3],mmax[3];
    for(i=0;i<3;i++){
      mmin[i]=rgb_below_min[i];
      mmax[i]=rgb_above_max[i];
    }
    fprintf(fileout,"EXTREMECOLORS\n");
    fprintf(fileout," %i %i %i %i %i %i\n",
     mmin[0],mmin[1],mmin[2],
     mmax[0],mmax[1],mmax[2]);
  }
  if(ncolorbars>ndefaultcolorbars){
	  colorbardata *cbi;
	  unsigned char *rrgb;
	  int n;

    fprintf(fileout,"GCOLORBAR\n");
    fprintf(fileout," %i\n",ncolorbars-ndefaultcolorbars);
    for(n=ndefaultcolorbars;n<ncolorbars;n++){
      cbi = colorbarinfo + n;
      fprintf(fileout," %s\n",cbi->label);
      fprintf(fileout," %i %i\n",cbi->nnodes,cbi->nodehilight);
      for(i=0;i<cbi->nnodes;i++){
        rrgb = cbi->rgb_node+3*i;
        fprintf(fileout," %i %i %i %i\n",cbi->index_node[i],(int)rrgb[0],(int)rrgb[1],(int)rrgb[2]);
      }
    }
  }
  fprintf(fileout,"\nTOUR INFO\n");
  fprintf(fileout,"---------\n\n");
  fprintf(fileout,"VIEWTOURFROMPATH\n");
  fprintf(fileout," %i\n",viewtourfrompath);
  fprintf(fileout,"VIEWALLTOURS\n");
  fprintf(fileout," %i\n",viewalltours);
  fprintf(fileout,"SHOWTOURROUTE\n");
  fprintf(fileout," %i\n",edittour);
  fprintf(fileout,"SHOWPATHNODES\n");
  fprintf(fileout," %i\n",show_path_knots);
  fprintf(fileout,"TOURCONSTANTVEL\n");
  fprintf(fileout," %i\n",tour_constant_vel);
//  fprintf(fileout,"TOUR_AVATAR\n");
//  fprintf(fileout," %i %f %f %f %f\n",
//    tourlocus_type,
//    tourcol_avatar[0],tourcol_avatar[1],tourcol_avatar[2],
//    tourrad_avatar);
  {
    keyframe *framei;
    float *col;
    int startup_count=0;
    int ii,uselocalspeed=0;

    fprintf(fileout,"TOURCOLORS\n");
    col=tourcol_selectedpathline;
    fprintf(fileout," %f %f %f   :selected path line\n",col[0],col[1],col[2]);
    col=tourcol_selectedpathlineknots;
    fprintf(fileout," %f %f %f   :selected path line knots\n",col[0],col[1],col[2]);
    col=tourcol_selectedknot;
    fprintf(fileout," %f %f %f   :selected knot\n",col[0],col[1],col[2]);
    col=tourcol_pathline;
    fprintf(fileout," %f %f %f   :path line\n",col[0],col[1],col[2]);
    col=tourcol_pathknots;
    fprintf(fileout," %f %f %f   :path knots\n",col[0],col[1],col[2]);
    col=tourcol_text;
    fprintf(fileout," %f %f %f   :text\n",col[0],col[1],col[2]);
    col=tourcol_avatar;
    fprintf(fileout," %f %f %f   :avatar\n",col[0],col[1],col[2]);

    if(flag==LOCAL_INI){
      fprintf(fileout,"TOURINDEX\n");
      fprintf(fileout," %i\n",selectedtour_index);
      startup_count=0;
      for(i=0;i<ntours;i++){
        tourdata *touri;

        touri = tourinfo + i;
        if(touri->startup==1)startup_count++;
      }
      for(ii=0;ii<2;ii++){
        if(ii==0&&ntours-startup_count>0){
          fprintf(fileout,"TOURS\n");
          fprintf(fileout," %i\n",ntours-startup_count);
        }
        else if(ii==1&&startup_count>0){
          fprintf(fileout,"DEFAULTTOURS\n");
          fprintf(fileout," %i\n",startup_count);
        }
        else{
          continue;
        }
        for(i=0;i<ntours;i++){
          tourdata *touri;

          touri = tourinfo + i;
          if(ii==1&&touri->startup==0)continue;
          if(ii==0&&touri->startup==1)continue;

          trim(touri->label);
          fprintf(fileout," %s\n",touri->label);
          fprintf(fileout," %i %i %f %i %i\n",
            touri->nkeyframes,touri->global_tension_flag,touri->global_tension,touri->glui_avatar_index,touri->display);
          for(framei=&touri->first_frame;framei!=&touri->last_frame;framei=framei->next){
            if(framei==&touri->first_frame)continue;
            sprintf(buffer,"%f %f %f %f ",
              framei->noncon_time,
              DENORMALIZE_X(framei->nodeval.eye[0]),
              DENORMALIZE_Y(framei->nodeval.eye[1]),
              DENORMALIZE_Z(framei->nodeval.eye[2]));
            trimmzeros(buffer);
            fprintf(fileout," %s %i ",buffer,framei->viewtype);
            if(framei->viewtype==0){
              sprintf(buffer,"%f %f %f %f %f %f %f ",
                framei->az_path,framei->nodeval.elev_path,framei->bank,
                framei->tension, framei->bias, framei->continuity,
                framei->nodeval.zoom);
            }
            else{
              sprintf(buffer,"%f %f %f %f %f %f %f ",
                DENORMALIZE_X(framei->nodeval.aview[0]),
                DENORMALIZE_Y(framei->nodeval.aview[1]),
                DENORMALIZE_Z(framei->nodeval.aview[2]),
                framei->tension, framei->bias, framei->continuity,
                framei->nodeval.zoom);
            }
            trimmzeros(buffer);
            fprintf(fileout," %s %i\n",buffer,uselocalspeed);
          }
        }
      }
    }
  }
  
  if(flag==LOCAL_INI){
    scriptfiledata *scriptfile;
    inifiledata *inifile;
    int first_time=1;
    FILE *fileout_inilist=NULL;

    for(scriptfile=first_scriptfile.next;scriptfile->next!=NULL;scriptfile=scriptfile->next){
      char *file;

      file=scriptfile->file;
      if(file!=NULL){
        fprintf(fileout,"SCRIPTFILE\n");
        fprintf(fileout," %s\n",file);
      }
    }
    for(inifile=first_inifile.next;inifile->next!=NULL;inifile=inifile->next){
      char ini_listfile[1024];

      if(inifile->file==NULL)continue;
      if(first_time==1){
        first_time=0;
        strcpy(ini_listfile,caseinifilename);
        strcat(ini_listfile,"list");
        fileout_inilist=fopen(ini_listfile,"w");
      }
      if(fileout_inilist!=NULL){
        fprintf(fileout_inilist,"INIFILE\n");
        fprintf(fileout_inilist," %s\n",inifile->file);
      }
    }
    if(fileout_inilist!=NULL)fclose(fileout_inilist);
  }
  {
    int svn_num;
    char version[256];

    getPROGversion(version);
    svn_num=getmaxrevision();    // get svn revision number
    fprintf(fileout,"\n\n# Development Environment\n");
    fprintf(fileout,"# -----------------------\n\n");
    fprintf(fileout,"# Smokeview Version: %s\n",version);
    fprintf(fileout,"# Smokeview Revision Number: %i\n",svn_num);
    fprintf(fileout,"# Smokeview Compile Date: %s\n",__DATE__);
    if(use_graphics==1){
      char version_label[256];
      char *glversion=NULL;

      glversion=(char *)glGetString(GL_VERSION);
      if(glversion!=NULL){
        strcpy(version_label,"OpenGL Version: "); 
        strcat(version_label,glversion);
        fprintf(fileout,"# %s\n",version_label);
      }
    }
    if(revision_fds>0){
      fprintf(fileout,"# FDS Revision Number: %i\n",revision_fds);
    }
#ifdef X64
    fprintf(fileout,"# Platform: WIN64\n");
#endif
#ifdef WIN32
#ifndef X64
    fprintf(fileout,"# Platform: WIN32\n");
#endif
#endif
#ifndef pp_OSX64
#ifdef pp_OSX
    printf("Platform: OSX\n");
#endif
#endif
#ifdef pp_OSX64
    printf("Platform: OSX64\n");
#endif
#ifndef pp_LINUX64
#ifdef pp_LINUX
    fprintf(fileout,"# Platform: LINUX\n");
#endif
#endif
#ifdef pp_LINUX64
    fprintf(fileout,"# Platform: LINUX64\n");
#endif

    if(use_graphics==1){
      GLint nred, ngreen, nblue, ndepth, nalpha;

      glGetIntegerv(GL_RED_BITS,&nred);    
      glGetIntegerv(GL_GREEN_BITS,&ngreen);
      glGetIntegerv(GL_BLUE_BITS,&nblue); 
      glGetIntegerv(GL_DEPTH_BITS,&ndepth);
      glGetIntegerv(GL_ALPHA_BITS,&nalpha);
      fprintf(fileout,"\n\n# Graphics Environment\n");
      fprintf(fileout,"# --------------------\n\n");
      fprintf(fileout,"#   Red bits:%i\n",nred);
      fprintf(fileout,"# Green bits:%i\n",ngreen);
      fprintf(fileout,"#  Blue bits:%i\n",nblue);
      fprintf(fileout,"# Alpha bits:%i\n",nalpha);
      fprintf(fileout,"# Depth bits:%i\n\n",ndepth);
    }
  }

  if(fileout!=stdout)fclose(fileout);
}

/* ------------------ update_loaded_lists ------------------------ */

void update_loaded_lists(void){
  int i;
  slicedata *slicei;
  patchdata *patchi;

  nslice_loaded=0;
  for(i=0;i<nsliceinfo;i++){
    slicei = sliceinfo + i;
    if(slicei->loaded==1){
      slice_loaded_list[nslice_loaded]=i;
      nslice_loaded++;
    }
  }

  npatch_loaded=0;
  for(i=0;i<npatchinfo;i++){
    patchi = patchinfo + i;
    if(patchi->loaded==1){
      patch_loaded_list[npatch_loaded]=i;
      npatch_loaded++;
    }
  }

}

/* ------------------ get_elevaz ------------------------ */

void get_elevaz(float *xyznorm,float *dtheta,float *rotate_axis, float *dpsi){
  float pi;

  // cos(dtheta) = (xyznorm .dot. vec3(0,0,1))/||xyznorm||
  // rotate_axis = xyznorm .cross. vec3(0,0,1)

  pi=4.0*atan(1.0);
  normalize(xyznorm,3);
  *dtheta = 180.0*acos(xyznorm[2])/pi;
  rotate_axis[0]=-xyznorm[1];
  rotate_axis[1]= xyznorm[0];
  rotate_axis[2]=0.0;
  normalize(rotate_axis,2);
  if(dpsi!=NULL){
    float xyznorm2[2];

    xyznorm2[0]=xyznorm[0];
    xyznorm2[1]=xyznorm[1];
    normalize(xyznorm2,2);
    *dpsi = 180.0*acos(xyznorm2[1])/pi;
    if(xyznorm2[0]<0.0)*dpsi=-(*dpsi);
  }
}

/* ------------------ get_filesize ------------------------ */

FILE_SIZE get_filesize(const char *filename){
  STRUCTSTAT statbuffer;
  int statfile;
  FILE_SIZE return_val;

  return_val=0;
  if(filename==NULL)return return_val;
  statfile=STAT(filename,&statbuffer);
  if(statfile!=0)return return_val;
  return_val = statbuffer.st_size;
  return return_val;
}

/* ------------------ get_labels ------------------------ */

void get_labels(char *buffer, int kind, char **label1, char **label2, char prop_buffer[255]){
  char *tok0, *tok1, *tok2;

  tok0=NULL;
  tok1=NULL;
  tok2=NULL;
  tok0 = strtok(buffer,"%");
  if(tok0!=NULL)tok1=strtok(NULL,"%");
  if(tok1!=NULL)tok2=strtok(NULL,"%");
  if(tok1!=NULL){
    trim(tok1);
    tok1=trim_front(tok1);
    if(strlen(tok1)==0)tok1=NULL;
  }
  if(tok2!=NULL){
    trim(tok2);
    tok2=trim_front(tok2);
    if(strlen(tok2)==0)tok2=NULL;
  }
  if(label2!=NULL){
    if(tok2==NULL&&kind==HUMANS){
      strcpy(prop_buffer,"Human_props(default)");
      *label2=prop_buffer;
    }
    else{
      *label2=tok2;
    }
  }
  if(label1!=NULL)*label1=tok1;
}

/* ------------------ get_prop_id ------------------------ */

propdata *get_prop_id(char *prop_id){
  int i;

  if(propinfo==NULL||prop_id==NULL||strlen(prop_id)==0)return NULL;
  for(i=0;i<npropinfo;i++){
    propdata *propi;

    propi = propinfo + i;

    if(strcmp(propi->label,prop_id)==0)return propi;
  }
  return NULL;
}

/* ------------------ init_evac_prop ------------------------ */

void init_evac_prop(void){
  char label[256];
  char *smokeview_id;
  int nsmokeview_ids;

  strcpy(label,"evac default");

  NewMemory( (void **)&prop_evacdefault,sizeof(propdata));
  nsmokeview_ids=1;

  init_prop(prop_evacdefault,nsmokeview_ids,label);


  NewMemory((void **)&smokeview_id,6+1);
  strcpy(smokeview_id,"sensor");
  prop_evacdefault->smokeview_ids[0]=smokeview_id;
  prop_evacdefault->smv_objects[0]=get_SVOBJECT_type(prop_evacdefault->smokeview_ids[0],missing_device);

  prop_evacdefault->smv_object=prop_evacdefault->smv_objects[0];
  prop_evacdefault->smokeview_id=prop_evacdefault->smokeview_ids[0];

  prop_evacdefault->ntextures=0;
}

/* ------------------ init_evac_prop ------------------------ */

surfdata *get_surface(char *label){
  int i;
  
  for(i=0;i<nsurfinfo;i++){
    surfdata *surfi;

    surfi = surfinfo + i;
    if(strcmp(surfi->surfacelabel,label)==0)return surfi;
  }
  return surfacedefault;
}
