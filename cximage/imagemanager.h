#ifndef _ImageManager_H
#define _ImageManager_H

#include "shapebase.h"
#include "Image.h"





#define  OBJSCANNNUM  1024*1024*4//307200//20480//20480//10240//
#define  OBJCOLLECTIONNUM  1024*1024*4//307200//20480//20480

#define BACKIMAGEWITH 2048//1024
#define BACKIMAGEHIGH 1536//1024

#define TRANSFERIMAGEWITH 320
#define TRANSFERIMAGEHIGH 320

#define COLORNUMSUM 256
class FindObject;

class ImageManager
{
public:

    ImageManager(){CurMode();}
    ~ImageManager();

    static int m_curmodul;
    static int m_imodulid;
    static void CurMode();
    static int GetCurMode();
    static bool EnsureAlgorithmRuntimeResources(int image_width, int image_height);

    static Image* m_pBackImage;
    static Image* m_pBackObjectImage;
    static Image* m_pMapImage;
    static Image* m_pModelImage;
    static Image* m_pTransferImage;

    static Image* m_pPyrImage0;
    static Image* m_pPyrImage1;
    static Image* m_pPyrImage2;

     int m_ishow;
     int m_iobjectshow;

    void setshow(int ishow);
    void setobjectshow(int ishow);

    int getshow();

 //   void draw(QPainter &painter);
 //   void drawtable(QPainter &painter);

    static void CreateBackImage(int iw,int ih);

    static Image* GetBackImage(int curmodul=1);


    static Image& BackImage();


    static void CreateBackObjectImage(int iw,int ih);

    static Image* GetBackObjectImage(int curmodul=1);

    static Image& BackObjectImage();


    static void CreateMapImage(int iw,int ih);

    static Image* GetMapImage(int curmodul=1);

    static Image& MapImage();


    static void CreateModelImage(int iw,int ih);

    static Image* GetModelImage(int curmodul=1);

    static Image& ModelImage();


    static void CreatePyrDownImage(int iw, int ih);

    static Image* GetPyrDownImage(int curmodul = 1,int ilevel=0);

    static Image& PyrDownImage0();
    static Image& PyrDownImage1();
    static Image& PyrDownImage2();


    static void CreateTransferImage(int iw,int ih);

    static Image* GetTransferImage(int curmodul=1);

    static Image& TransferImage();


    static gp_Pnt* _thelistscanorA;
//    static gp_Pnt * _thelistscanorB;
//    static gp_Pnt * _thelistscanorC;
//    static gp_Pnt * _thelistscanorD;

    static gp_Pnt* _thelistcollectorA;
//    static gp_Pnt * _thelistcollectorB;
//    static gp_Pnt * _thelistcollectorC;
//    static gp_Pnt * _thelistcollectorD;

    static gp_Pnt*GetListScan(int curmodul=1);
    static gp_Pnt*GetListCollect(int curmodul=1);


     static FindObject * m_pfindobject;

     static FindObject* Getbackfindobject(int curmodul = 1);


    static int64 *g_r_table;
    static int64*g_g_table;
    static int64*g_b_table;

    static int64* GetRtable(int curmodul=1);
    static int64* GetGtable(int curmodul=1);
    static int64* GetBtable(int curmodul=1);
    
    static int g_r_table_thre;
    static int g_g_table_thre;
    static int g_b_table_thre;

    static int g_table_rate;
 
 
};



#endif //_ImageManager_H
