#include "pch.h"

#include "imagemanager.h"
#include "findobject.h"
Image* ImageManager::m_pBackImage = 0;
Image* ImageManager::m_pBackObjectImage = 0;
Image* ImageManager::m_pMapImage = 0;
Image*ImageManager::m_pModelImage =0;
Image*ImageManager::m_pTransferImage =0;

Image* ImageManager::m_pPyrImage0 = 0;
Image* ImageManager::m_pPyrImage1 = 0;
Image* ImageManager::m_pPyrImage2 = 0;
 
FindObject* ImageManager::m_pfindobject =0;
 
int ImageManager::m_imodulid = 0;

int ImageManager::m_curmodul = 0;

gp_Pnt* ImageManager::_thelistscanorA = 0;

gp_Pnt* ImageManager::_thelistcollectorA = 0;

int64 * ImageManager::g_r_table = 0;
int64 * ImageManager::g_g_table = 0;
int64 * ImageManager::g_b_table = 0;

int ImageManager::g_r_table_thre = 0;
int ImageManager::g_g_table_thre = 0;
int ImageManager::g_b_table_thre = 0;

int ImageManager::g_table_rate = 0;



ImageManager::~ImageManager()
{
    if(NULL!=_thelistcollectorA)
    {
        delete []_thelistcollectorA;
        _thelistcollectorA = NULL;
    }
    if(NULL!=_thelistscanorA)
    {
        delete []_thelistscanorA;
        _thelistscanorA = NULL;
    }
    m_imodulid = 0;

    m_curmodul = 0;

}
void ImageManager::setobjectshow(int ishow)
{
    m_iobjectshow = ishow;
//    if(0!=m_pfindobject)
//        m_pfindobject->setshow(m_iobjectshow);
}
/*
void ImageManager::draw(QPainter &painter)
{
    if(0x02==m_ishow)
    {
        if(0!=m_pBackImage)
        {
            m_pBackImage->draw(painter);
        }
    }
    else if(0x04==m_ishow)
    {
        if(0!=m_pMapImage)
        {
            m_pMapImage->draw(painter);
        }
    }
    else if(0x08==m_ishow)
    {
        if(0!=m_pModelImage)
        {
            m_pModelImage->draw(painter);
        }
    }
    else if(0x10==m_ishow)
    {
        if(0!=m_pBackObjectImage)
        {
            m_pBackObjectImage->draw(painter);
        }
    }
    else if(0x20==m_ishow)
    {
        drawtable(painter);
    }
    else if(0x40==m_ishow)
    {
        QPalette apalette;
        m_pfindobject->drawshape(painter,apalette);
    }
}
 
void ImageManager::drawtable(QPainter &painter)
{
    if(g_r_table==0
        ||g_g_table==0
        ||g_b_table==0)
        return;
    int ibx = 0;
    int iby = 0;

    int irate = 1;
    if(g_table_rate>30000)
        irate = 10;
    else if(g_table_rate>10000)
        irate = 5;

    QPainterPath pathr;
    pathr.moveTo(0, 300);
    for(int i=0;i<=255;i++)
    {
        pathr.lineTo(i, 300-(g_r_table[i]/irate));
    }

    QPainterPath pathg;
    pathg.moveTo(0, 300);
    for(int i=0;i<=255;i++)
    {
        pathg.lineTo(i, 300-(g_g_table[i]/irate));
    }

    QPainterPath pathb;
    pathb.moveTo(0, 300);
    for(int i=0;i<=255;i++)
    {
        pathb.lineTo(i, 300-(g_b_table[i]/irate));
    }

painter.save();
    painter.setPen(Qt::red);
    painter.drawRect(ibx+0,iby+0,256,300);
    painter.drawPath(pathr);

    painter.drawLine(ibx+g_r_table_thre,iby+0,ibx+g_r_table_thre,iby+300);
painter.restore();


painter.save();
painter.translate(256, 0);
    painter.setPen(Qt::green);
    painter.drawRect(ibx+0,iby+0,256,300);
    painter.drawPath(pathg);
    painter.drawLine(ibx+g_g_table_thre,iby+0,ibx+g_g_table_thre,iby+300);

painter.restore();

painter.save();
painter.translate(512, 0);
    painter.setPen(Qt::blue);
    painter.drawRect(ibx+0,iby+0,256,300);
    painter.drawPath(pathb);
    painter.drawLine(ibx+g_b_table_thre,iby+0,ibx+g_b_table_thre,iby+300);

painter.restore();
}
*/

void ImageManager::setshow(int ishow)
{
    m_ishow = ishow;
}

int ImageManager::getshow()
{
    return m_ishow;
}

void ImageManager::CurMode()
{
    m_imodulid = m_imodulid + 1;
}

int ImageManager::GetCurMode()
{
    return m_imodulid;
}

bool ImageManager::EnsureAlgorithmRuntimeResources(int image_width, int image_height)
{
    // The current cximage implementation has one concrete resource module.
    // Initialize it once for serial Manual/Headless/Suite execution; do not
    // call CurMode repeatedly because it increments the module id.
    if (m_imodulid == 0)
        m_imodulid = 1;
    if (m_imodulid != 1)
        return false;

    const int required_width = std::max(BACKIMAGEWITH, image_width + 8);
    const int required_height = std::max(BACKIMAGEHIGH, image_height + 8);

    CreateBackImage(required_width, required_height);
    CreateBackObjectImage(required_width, required_height);
    CreateMapImage(required_width, required_height);

    if (m_pBackImage != nullptr &&
        (m_pBackImage->getWidth() < required_width || m_pBackImage->getHeight() < required_height))
        *m_pBackImage = Image(required_width, required_height, CV_8UC3);
    if (m_pBackObjectImage != nullptr &&
        (m_pBackObjectImage->getWidth() < required_width || m_pBackObjectImage->getHeight() < required_height))
        *m_pBackObjectImage = Image(required_width, required_height, CV_8UC3);
    if (m_pMapImage != nullptr &&
        (m_pMapImage->getWidth() < required_width || m_pMapImage->getHeight() < required_height))
        *m_pMapImage = Image(required_width, required_height, CV_8UC4);

    gp_Pnt* scan = GetListScan(1);
    gp_Pnt* collect = GetListCollect(1);
    FindObject* find_object = Getbackfindobject(1);

    return m_pBackImage != nullptr &&
        m_pBackObjectImage != nullptr &&
        m_pMapImage != nullptr &&
        scan != nullptr &&
        collect != nullptr &&
        find_object != nullptr;
}

Image* ImageManager::GetBackImage(int curmodul)
{
    if(1==curmodul)
    {
        if(0==m_pBackImage)
        {
            CreateBackImage(BACKIMAGEWITH,BACKIMAGEHIGH);
        }
        return m_pBackImage;
    }
    else if(2==curmodul)
    {

    }
    return nullptr;
}
void ImageManager::CreateBackImage(int iw,int ih)
{
    if(0!=m_pBackImage)
        return;
    Image&backimage = BackImage();
    m_pBackImage = &backimage;
    Image aimage(iw,ih, CV_8UC3);
    *m_pBackImage = aimage;
}
Image& ImageManager::BackImage()
{
    static Image _thebackimage;
    return _thebackimage;
}


Image* ImageManager::GetBackObjectImage(int curmodul)
{
    if(1==curmodul)
    {
        if(0==m_pBackObjectImage)
        {
            CreateBackObjectImage(BACKIMAGEWITH,BACKIMAGEHIGH);
        }
        return m_pBackObjectImage;
    }
    else if(2==curmodul)
    {

    }
    return 0;
}
void ImageManager::CreateBackObjectImage(int iw,int ih)
{
    if(0!=m_pBackObjectImage)
        return;
    Image&backobjectimage = BackObjectImage();
    m_pBackObjectImage = &backobjectimage;
    Image aimage(iw, ih, CV_8UC3);
    *m_pBackObjectImage = aimage;
}
Image& ImageManager::BackObjectImage()
{
    static Image _thebackobjectimage;
    return _thebackobjectimage;
}

Image* ImageManager::GetMapImage(int curmodul)
{
    if(1==curmodul)
    {
        if(0==m_pMapImage)
        {
            CreateMapImage(BACKIMAGEWITH,BACKIMAGEHIGH);
        }
        return m_pMapImage;
    }
    else if(2==curmodul)
    {

    }
    return 0;
}
void ImageManager::CreateMapImage(int iw,int ih)
{
    if(0!=m_pMapImage)
        return;
    Image&mapimage = MapImage();
    m_pMapImage = &mapimage;
    Image aimage(iw, ih, CV_8UC4);
    *m_pMapImage = aimage;
}
Image& ImageManager::MapImage()
{
    static Image _themapimage;
    return _themapimage;

}

Image* ImageManager::GetModelImage(int curmodul)
{
    if(1==curmodul)
    {
        if(0==m_pModelImage)
        {
            CreateModelImage(BACKIMAGEWITH,BACKIMAGEHIGH);
        }
        return m_pModelImage;
    }
    else if(2==curmodul)
    {

    }
    return 0;
}
void ImageManager::CreateModelImage(int iw,int ih)
{
    if(0!=m_pModelImage)
        return;
    Image&modelimage = ModelImage();
    m_pModelImage = &modelimage;
    Image aimage(iw, ih, CV_8UC3);
    *m_pModelImage = aimage;
}
Image& ImageManager::ModelImage()
{
    static Image _themodelimage;
    return _themodelimage;

}

Image* ImageManager::GetPyrDownImage(int curmodul,int ilevel)
{
    if (1 == curmodul)
    {
        if (0 == m_pTransferImage)
        {
            CreateTransferImage(BACKIMAGEWITH, BACKIMAGEHIGH);
        }
        if (0 == ilevel)
            return m_pPyrImage0;
        else if (1 == ilevel)
            return m_pPyrImage1;
        else if (2 == ilevel)
            return m_pPyrImage2;
    }
    else if (2 == curmodul)
    {

    } 
    return nullptr;
}

void ImageManager::CreatePyrDownImage(int iw, int ih)
{ 
    if (0 != m_pPyrImage0)
        return;
    Image& pyrimage0 = PyrDownImage0();
    m_pPyrImage0 = &pyrimage0;

    Image aimage0(iw, ih, CV_8UC3);
    *m_pPyrImage0 = aimage0;

    Image& pyrimage1 = PyrDownImage1();
    m_pPyrImage1 = &pyrimage1;

    Image aimage1(iw, ih, CV_8UC3);
    *m_pPyrImage1 = aimage1;

    Image& pyrimage2 = PyrDownImage2();
    m_pPyrImage2 = &pyrimage2;

    Image aimage2(iw, ih, CV_8UC3);
    *m_pPyrImage2 = aimage2; 
}

Image& ImageManager::PyrDownImage0()
{
    static Image _thePyrimage0;

    return _thePyrimage0;
}
Image& ImageManager::PyrDownImage1()
{
    static Image _thePyrimage1;

    return _thePyrimage1;
     
}
Image& ImageManager::PyrDownImage2()
{
    static Image _thePyrimage2;

    return _thePyrimage2;
}



Image* ImageManager::GetTransferImage(int curmodul)
{
    if(1==curmodul)
    {
        if(0==m_pTransferImage)
        {
            CreateTransferImage(BACKIMAGEWITH,BACKIMAGEHIGH);
        }
        return m_pTransferImage;
    }
    else if(2==curmodul)
    {

    }
    return 0;
}
void ImageManager::CreateTransferImage(int iw,int ih)
{
    if(0!=m_pTransferImage)
        return;
    Image&transferimage = TransferImage();
    m_pTransferImage = &transferimage;
    Image aimage(iw, ih, CV_8UC3);
    *m_pTransferImage = aimage;
}
Image& ImageManager::TransferImage()
{
    static Image _thetransferimage;
    return _thetransferimage;

}
gp_Pnt* ImageManager::GetListScan(int curmodul)
{
    switch(curmodul)
    {
        default:
        case 1:
             if(NULL==_thelistscanorA)
             {
                _thelistscanorA = new gp_Pnt[OBJSCANNNUM];
             }
            return _thelistscanorA;
            break;
    }
}
gp_Pnt* ImageManager::GetListCollect(int curmodul)
{
    switch(curmodul)
    {
    default:
    case 1:
        if(NULL==_thelistcollectorA)
        {
            _thelistcollectorA = new gp_Pnt[OBJCOLLECTIONNUM];
        }
        return _thelistcollectorA;
        break;
    }
}
 
FindObject* ImageManager::Getbackfindobject(int curmodul)
{
    if(1==curmodul)
    {
        if(0==m_pfindobject)
        {

            static FindObject _thefindobject;

            m_pfindobject = &_thefindobject;
        }
        return m_pfindobject;
    }

    return 0;
}
 
int64* ImageManager::GetRtable(int curmodul)
{
    switch(curmodul)
    {
        default:
        case 1:
             if(NULL==g_r_table)
             {
                g_r_table = new int64[COLORNUMSUM];
             }
            return g_r_table;
            break;
    }
}
int64* ImageManager::GetGtable(int curmodul)
{
    switch(curmodul)
    {
        default:
        case 1:
             if(NULL==g_g_table)
             {
                g_g_table = new int64[COLORNUMSUM];
             }
            return g_g_table;
            break;
    }
}
int64* ImageManager::GetBtable(int curmodul)
{
    switch(curmodul)
    {
        default:
        case 1:
             if(NULL==g_b_table)
             {
                g_b_table = new int64[COLORNUMSUM];
             }
            return g_b_table;
            break;
    }
}






