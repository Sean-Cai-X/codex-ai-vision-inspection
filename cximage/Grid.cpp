#include "pch.h"

#include "FindLine.h"
#include "occtinclude.h"
#include "imagemanager.h"
#include "findobject.h"
#include "Grid.h"

Grid::Grid() :
    m_imapmodelw(0),
    m_imapmodelh(0),
    m_iunitw(12),
    m_iunith(12),
    m_iorgx(0),
    m_iorgy(0)
{
    m_imodelmatrixsum = 0;

    m_irealw = 640;
    m_irealh = 480;

    m_ishowpicw = 80;
    m_ishowpich = 80;
     

    int icurmodule = ImageManager::GetCurMode();
    g_pbackfindobject = ImageManager::Getbackfindobject(icurmodule);

}
Grid::~Grid()
{

}
int Grid::show() const
{
    return m_ishow;
}
void Grid::setshow(int ishow)
{
    m_ishow = ishow;
}
void Grid::setroi(int iorgx, int iorgy, int iw, int ih)
{
    m_iorgx = iorgx;
    m_iorgy = iorgy;
    m_iw = iw;
    m_ih = ih;
}
void Grid::setgrid(int ih, int iw, int iwsum, int ihsum, int ihgap, int iwgap)
{
    m_igridw = iw;
    m_igridh = ih;
    m_iwgridsum = iwsum;
    m_ihgridsum = ihsum;
    m_iwgridgap = iwgap;
    m_ihgridgap = ihgap;
    m_gridT.clear();
    m_grid.clear();
    int ibeginx = 0;
    int ibeginy = 0;
    int icurx = 0;
    int icury = 0;
    for (int ish = 0; ish < ihsum; ish++)
    {
        for (int isw = 0; isw < iwsum; isw++)
        {
            icurx = ibeginx + iwgap * isw;
            icury = ibeginy + ihgap * ish;
            gp_Rectangle arect(gp_Pnt(icurx, icury,0), iw, ih);
            char chnum[25];
            sprintf(chnum, "%d,%d", isw, ish);
            m_gridT[std::string(chnum)] = eGRIDType::GRIDType_NONE;
            m_grid[std::string(chnum)].x = arect.TopLeft();
        }
    }
}
void Grid::drawshape()
{
 
}

void Grid::setfastlistvalue(int inum, int ivalue)
{
    if (inum < m_fastmodel.size())
        m_fastmodel[inum] = ivalue;
}
void Grid::clear()
{
    GridType::iterator pIter;
    for (pIter = m_gridT.begin(); pIter != m_gridT.end(); pIter++)
    {
        m_gridT[pIter->first] = 0;
    }

}
void Grid::settype(int igdix, int igdiy, int ivalue)
{
    char chnum[25];
    sprintf(chnum, "%d,%d", igdix, igdiy);
    m_gridT[std::string(chnum)] = ivalue;
}
void Grid::settypevalue(int igdix, int igdiy, int ivalue)
{
    char chnum[25];
    sprintf(chnum, "%d,%d", igdix, igdiy);
    m_gridT[std::string(chnum)] = ivalue;
}
int Grid::gettypevalue(int igridx, int igridy)
{
    char chnum[25];
    sprintf(chnum, "%d,%d", igridx, igridy);
    return m_gridT[std::string(chnum)];
}
void Grid::setbrush(int itype, int icolor)
{ 
 
}

void Grid::savemodelfile(const char* pchar)
{ 
    m_modelpoints.save(pchar);
}
void Grid::loadmodelfile(const char* pchar)
{
    m_modelpoints.load(pchar);
}
void Grid::modeltogrid(int iatype, int ibtype)
{
    if (iatype == ibtype)
    {
        iatype = 2;
        ibtype = 4;
    }
    gp_Path path = m_modelpoints.getpath();

    const int icount = static_cast<int>(path.ElementCount());

    for (int i = 0; i < icount - 1; i++)
    {
        gp_Pnt aele = path.ElementAt(i);
        settype(aele.X(), aele.Y(), iatype);
        i++;
        aele = path.ElementAt(i);
        settype(aele.X(), aele.Y(), ibtype);
    }
}

void Grid::roiimagetomodel(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    ROIImagetoModel(*pgetimage);
}
void Grid::ROIImagetoModel(Image& aimage)
{
    m_fastmodel.clear();
    int ix0 = aimage.ix0();
    int iy0 = aimage.iy0();
    int iw = aimage.iw();
    int ih = aimage.ih();
    {
        m_imapmodelw = iw;
        m_imapmodelh = ih;
    }

    for (int j = 0; j < ih; j++)
    {
        for (int i = 0; i < iw; i++)
        {
            cv::Vec3b argb = aimage.pixel(ix0 + i, iy0 + j);
            int itype = Red(argb);
            if (itype > 0)
                m_fastmodel.push_back(1);
            else
                m_fastmodel.push_back(0);
        }
    }
}
void Grid::ROIImagetoModel_gray(Image& aimage)
{
    m_fastmodel.clear();
    int ix0 = aimage.ix0();
    int iy0 = aimage.iy0();
    int iw = aimage.iw();
    int ih = aimage.ih();
    {
        m_imapmodelw = iw;
        m_imapmodelh = ih;
    }

    for (int j = 0; j < ih; j++)
    {
        for (int i = 0; i < iw; i++)
        {
            cv::Vec3b argb = aimage.pixel(ix0 + i, iy0 + j);
            int itype = Red(argb);
            m_fastmodel.push_back(itype);
        }
    }
}

void Grid::ModelGridMethod_Gauss()
{
    vector<int> oldmodel = m_fastmodel;

    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (1 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -1;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -1;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -1;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -1;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -1;
                }
            }

        }
    }

    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = m_fastmodel[inum];
            if (-1 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);


                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -4;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -4;

                }

                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = m_fastmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = m_fastmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -4;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -4;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -4;
                }
            }

        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = m_fastmodel[inum];
            if (-4 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);


                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -8;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -8;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -8;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -8;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = m_fastmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -8;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = m_fastmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -8;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -8;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -8;
                }
            }

        }
    }

    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = m_fastmodel[inum];
            if (0 == ishownum)
                m_fastmodel[inum] = -10;

        }
    }

}
void Grid::ModelGridMethod_Object()
{
    vector<int> oldmodel = m_fastmodel;
    vector<int> mapmodel = oldmodel;

    vector<gp_Pnt> scanlist;
    vector<gp_Pnt> collectlist;
    int isetnum = 2;
    int isetnum0 = -1;
    collectlist.push_back(gp_Pnt(0, 0 ,0));
    int icurset = 0;

    int i1sum = 0;
    int i0sum = 0;
    for (int iz = 0; iz < collectlist.size(); iz++)
    {
        scanlist.clear();
        scanlist.push_back(collectlist[iz]);

        gp_Pnt apoint0 = collectlist[iz];
        int ix = apoint0.X();
        int iy = apoint0.Y();
        int ixycur = ix + iy * m_imapmodelw;
        int ixyvalue0 = oldmodel[ixycur];
        if (ixyvalue0 == 0)
        {
            i0sum = i0sum + 1;

            isetnum0 = isetnum0 - 1;
        }
        else if (ixyvalue0 == 1)
        {
            i1sum = i1sum + 1;

            isetnum = isetnum + 1;
        }
        else
            continue;


        for (int it = 0; it < scanlist.size(); it++)
        {
            gp_Pnt apoint1 = scanlist[it];
            int i = apoint1.X();
            int j = apoint1.Y();
            int icur = i + j * m_imapmodelw;
            int ivalue0 = m_fastmodel[icur];
            int iscaned = mapmodel[icur];
            if (iscaned != 2)
            {
                mapmodel[icur] = 2;

                if (ivalue0 == 0)
                    oldmodel[icur] = isetnum0;
                else if (ivalue0 == 1)
                    oldmodel[icur] = isetnum;

                icurset = oldmodel[icur];
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);

                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                    if (2 != mapmodel[iup])
                    {
                        int ivalue = oldmodel[iup];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[iup] = icurset;
                            scanlist.push_back(gp_Pnt(i, j - 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i, j - 1,0));
                        }
                    }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                    if (2 != mapmodel[idown])
                    {
                        int ivalue = oldmodel[idown];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[idown] = icurset;
                            scanlist.push_back(gp_Pnt(i, j + 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i, j + 1,0));
                        }
                    }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                    if (2 != mapmodel[ileft])
                    {
                        int ivalue = oldmodel[ileft];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[ileft] = icurset;
                            scanlist.push_back(gp_Pnt(i - 1, j,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i - 1, j,0));
                        }
                    }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                    if (2 != mapmodel[iright])
                    {
                        int ivalue = oldmodel[iright];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[iright] = icurset;
                            scanlist.push_back(gp_Pnt(i + 1, j,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i + 1, j,0));
                        }
                    }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                    if (2 != mapmodel[iupleft])
                    {
                        int ivalue = oldmodel[iupleft];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[iupleft] = icurset;
                            scanlist.push_back(gp_Pnt(i - 1, j - 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i - 1, j - 1,0));
                        }
                    }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                    if (2 != mapmodel[iupright])
                    {
                        int ivalue = oldmodel[iupright];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[iupright] = icurset;
                            scanlist.push_back(gp_Pnt(i + 1, j - 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i + 1, j - 1,0));
                        }
                    }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                    if (2 != mapmodel[idownleft])
                    {
                        int ivalue = oldmodel[idownleft];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[idownleft] = icurset;
                            scanlist.push_back(gp_Pnt(i - 1, j + 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i - 1, j + 1,0));
                        }
                    }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                    if (2 != mapmodel[idownright])
                    {
                        int ivalue = oldmodel[idownright];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[idownright] = icurset;
                            scanlist.push_back(gp_Pnt(i + 1, j + 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i + 1, j + 1,0));
                        }
                    }
            }
        }
    }

    easyobj eobj;
    eobj.s_ibobjnum = i0sum;
    eobj.s_iwobjnum = i1sum;

    m_fastmodel.clear();
    m_fastmodel = oldmodel;
}
void Grid::ModelGridMethod_Inside()
{
    vector<int> oldmodel = m_fastmodel;

    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (1 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -1;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -1;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -1;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -1;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -1;
                }
            }
            if (1 == ishownum)
                if (0 == i
                    || 0 == j
                    || m_imapmodelh - 1 == j
                    || m_imapmodelw - 1 == i)
                    m_fastmodel[inum] = -1;
        }
    }

    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (-1 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (1 == ivalue)
                        m_fastmodel[iupleft] = -1;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (1 == ivalue)
                        m_fastmodel[iupright] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (1 == ivalue)
                        m_fastmodel[idownleft] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (1 == ivalue)
                        m_fastmodel[idownright] = -1;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (1 == ivalue)
                        m_fastmodel[iup] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (1 == ivalue)
                        m_fastmodel[idown] = -1;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (1 == ivalue)
                        m_fastmodel[ileft] = -1;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (1 == ivalue)
                        m_fastmodel[iright] = -1;
                }
            }
        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (-1 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (1 == ivalue)
                        m_fastmodel[iupleft] = -2;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (1 == ivalue)
                        m_fastmodel[iupright] = -2;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (1 == ivalue)
                        m_fastmodel[idownleft] = -2;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (1 == ivalue)
                        m_fastmodel[idownright] = -2;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (1 == ivalue)
                        m_fastmodel[iup] = -2;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (1 == ivalue)
                        m_fastmodel[idown] = -2;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (1 == ivalue)
                        m_fastmodel[ileft] = -2;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (1 == ivalue)
                        m_fastmodel[iright] = -2;
                }
            }
        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (-2 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (1 == ivalue)
                        m_fastmodel[iupleft] = -3;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (1 == ivalue)
                        m_fastmodel[iupright] = -3;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (1 == ivalue)
                        m_fastmodel[idownleft] = -3;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (1 == ivalue)
                        m_fastmodel[idownright] = -3;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (1 == ivalue)
                        m_fastmodel[iup] = -3;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (1 == ivalue)
                        m_fastmodel[idown] = -3;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (1 == ivalue)
                        m_fastmodel[ileft] = -3;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (1 == ivalue)
                        m_fastmodel[iright] = -3;
                }
            }
        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (-3 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (1 == ivalue)
                        m_fastmodel[iupleft] = -4;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (1 == ivalue)
                        m_fastmodel[iupright] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (1 == ivalue)
                        m_fastmodel[idownleft] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (1 == ivalue)
                        m_fastmodel[idownright] = -4;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (1 == ivalue)
                        m_fastmodel[iup] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (1 == ivalue)
                        m_fastmodel[idown] = -4;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (1 == ivalue)
                        m_fastmodel[ileft] = -4;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (1 == ivalue)
                        m_fastmodel[iright] = -4;
                }
            }
        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (1 == ishownum)
            {
                m_fastmodel[inum] = -5;
            }
        }
    }


}
void Grid::ModelGridMethod_Outside()
{
    vector<int> oldmodel = m_fastmodel;

    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (0 != ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -1;
                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -1;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -1;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -1;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -1;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -1;
                }
            }
        }
    }

    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (-1 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -2;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -2;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -2;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -2;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -2;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -2;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -2;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -2;
                }
            }
        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (-2 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -3;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -3;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -3;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -3;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -3;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -3;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -3;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -3;
                }
            }
        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (-3 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -4;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -4;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -4;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -4;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -4;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -4;
                }
            }
        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (-4 == ishownum)
            {
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupleft];
                    if (0 == ivalue)
                        m_fastmodel[iupleft] = -5;

                }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[iupright];
                    if (0 == ivalue)
                        m_fastmodel[iupright] = -5;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownleft];
                    if (0 == ivalue)
                        m_fastmodel[idownleft] = -5;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = m_fastmodel[idownright];
                    if (0 == ivalue)
                        m_fastmodel[idownright] = -5;

                }


                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[iup];
                    if (0 == ivalue)
                        m_fastmodel[iup] = -5;
                }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                {
                    int ivalue = oldmodel[idown];
                    if (0 == ivalue)
                        m_fastmodel[idown] = -5;
                }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[ileft];
                    if (0 == ivalue)
                        m_fastmodel[ileft] = -5;
                }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                {
                    int ivalue = oldmodel[iright];
                    if (0 == ivalue)
                        m_fastmodel[iright] = -5;
                }
            }
        }
    }
    oldmodel = m_fastmodel;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = oldmodel[inum];
            if (0 == ishownum)
            {
                m_fastmodel[inum] = -6;
            }
        }
    }


}
int Grid::getfastmodelw()
{
    return m_imapmodelw;
}
int Grid::getfastmodelh()
{
    return m_imapmodelh;
}
int Grid::getfastmodelvalue(int ix, int iy)
{
    if (ix < m_imapmodelw
        && ix >= 0
        && iy < m_imapmodelh
        && iy >= 0)
        return m_fastmodel[ix + (iy * m_imapmodelw)];
    else
        return 0;
}
int Grid::getobject_areagrid_max()
{
    g_pbackfindobject->setminmaxarea(0, 9999);
    g_pbackfindobject->MeasureGrid(this);
    return 0;
}
int Grid::getobject_totalvalue_max()
{
    return 0;
}

easyobj Grid::ModelGridMethod_ObjectA()
{

    easyobj eobj;
    eobj.s_ibobjnum = 0;
    eobj.s_iwobjnum = 0;
    if (m_fastmodel.size() <= 0)
        return eobj;
    vector<int> oldmodel = m_fastmodel;
    vector<int> mapmodel = oldmodel;

    vector<gp_Pnt> scanlist;
    vector<gp_Pnt> collectlist;

    int isetnum = 2;
    int isetnum0 = -1;
    collectlist.push_back(gp_Pnt(0, 0,0));
    int icurset = 0;

    int i1sum = 0;
    int i0sum = 0;
    for (int iz = 0; iz < collectlist.size(); iz++)
    {
        scanlist.clear();
        scanlist.push_back(collectlist[iz]);

        gp_Pnt apoint0 = collectlist[iz];
        int ix = apoint0.X();
        int iy = apoint0.Y();
        int ixycur = ix + iy * m_imapmodelw;
        int ixyvalue0 = oldmodel[ixycur];
        if (ixyvalue0 == 0)
        {
            i0sum = i0sum + 1;

            isetnum0 = isetnum0 - 1;
        }
        else if (ixyvalue0 == 1)
        {
            i1sum = i1sum + 1;

            isetnum = isetnum + 1;
        }
        else
            continue;


        for (int it = 0; it < scanlist.size(); it++)
        {
            gp_Pnt apoint1 = scanlist[it];
            int i = apoint1.X();
            int j = apoint1.Y();
            int icur = i + j * m_imapmodelw;
            int ivalue0 = m_fastmodel[icur];
            int iscaned = mapmodel[icur];
            if (iscaned != 2)
            {
                mapmodel[icur] = 2;

                if (ivalue0 == 0)
                    oldmodel[icur] = isetnum0;
                else if (ivalue0 == 1)
                    oldmodel[icur] = isetnum;

                icurset = oldmodel[icur];
                int iup = i + ((j - 1) * m_imapmodelw);
                int idown = i + ((j + 1) * m_imapmodelw);
                int ileft = i - 1 + (j * m_imapmodelw);
                int iright = i + 1 + (j * m_imapmodelw);

                int iupleft = (i - 1) + ((j - 1) * m_imapmodelw);
                int iupright = (i + 1) + ((j - 1) * m_imapmodelw);
                int idownleft = (i - 1) + ((j + 1) * m_imapmodelw);
                int idownright = (i + 1) + ((j + 1) * m_imapmodelw);

                if (j - 1 >= 0 && j - 1 < m_imapmodelh)
                    if (2 != mapmodel[iup])
                    {
                        int ivalue = oldmodel[iup];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[iup] = icurset;
                            scanlist.push_back(gp_Pnt(i, j - 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i, j - 1,0));
                        }
                    }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh)
                    if (2 != mapmodel[idown])
                    {
                        int ivalue = oldmodel[idown];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[idown] = icurset;
                            scanlist.push_back(gp_Pnt(i, j + 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i, j + 1,0));
                        }
                    }
                if (i - 1 >= 0 && i - 1 < m_imapmodelw)
                    if (2 != mapmodel[ileft])
                    {
                        int ivalue = oldmodel[ileft];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[ileft] = icurset;
                            scanlist.push_back(gp_Pnt(i - 1, j,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i - 1, j,0));
                        }
                    }
                if (i + 1 >= 0 && i + 1 < m_imapmodelw)
                    if (2 != mapmodel[iright])
                    {
                        int ivalue = oldmodel[iright];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[iright] = icurset;
                            scanlist.push_back(gp_Pnt(i + 1, j,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i + 1, j,0));
                        }
                    }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                    if (2 != mapmodel[iupleft])
                    {
                        int ivalue = oldmodel[iupleft];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[iupleft] = icurset;
                            scanlist.push_back(gp_Pnt(i - 1, j - 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i - 1, j - 1,0));
                        }
                    }
                if (j - 1 >= 0 && j - 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                    if (2 != mapmodel[iupright])
                    {
                        int ivalue = oldmodel[iupright];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[iupright] = icurset;
                            scanlist.push_back(gp_Pnt(i + 1, j - 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i + 1, j - 1,0));
                        }
                    }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i - 1 >= 0 && i - 1 < m_imapmodelw)
                    if (2 != mapmodel[idownleft])
                    {
                        int ivalue = oldmodel[idownleft];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[idownleft] = icurset;
                            scanlist.push_back(gp_Pnt(i - 1, j + 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i - 1, j + 1,0));
                        }
                    }
                if (j + 1 >= 0 && j + 1 < m_imapmodelh
                    && i + 1 >= 0 && i + 1 < m_imapmodelw)
                    if (2 != mapmodel[idownright])
                    {
                        int ivalue = oldmodel[idownright];
                        if (ivalue0 == ivalue)
                        {
                            oldmodel[idownright] = icurset;
                            scanlist.push_back(gp_Pnt(i + 1, j + 1,0));
                        }
                        else
                        {
                            collectlist.push_back(gp_Pnt(i + 1, j + 1,0));
                        }
                    }
            }
        }
    }

    eobj.s_ibobjnum = i0sum;
    eobj.s_iwobjnum = i1sum;

    return eobj;

}
void Grid::ZeroModel()
{
    if (m_fastmodel.size() <= 0)
        return;
    int invalidy = -1;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + j * m_imapmodelw;
            if (1 == m_fastmodel[inum])
            {
                invalidy = j;
                break;
            }
        }
        if (invalidy != -1)
        {
            break;
        }
    }

    if (invalidy == -1)
        invalidy = 0;
    int invalidx = -1;
    for (int i = 0; i < m_imapmodelw; i++)
    {
        for (int j = 0; j < m_imapmodelh; j++)
        {
            int inum = i + j * m_imapmodelw;
            if (1 == m_fastmodel[inum])
            {
                invalidx = i;
                break;
            }
        }
        if (invalidx != -1)
        {
            break;
        }
    }
    if (invalidx == -1)
        invalidx = 0;
    for (int i = 0; i < m_imapmodelw - invalidx; i++)
    {
        for (int j = 0; j < m_imapmodelh; j++)
        {
            int inum0 = i + invalidx + j * m_imapmodelw;
            int inum = i + j * m_imapmodelw;
            m_fastmodel[inum] = m_fastmodel[inum0];
        }
    }
    for (int i = 0; i < invalidx; i++)
    {
        for (int j = 0; j < m_imapmodelh; j++)
        {
            int inum = j * m_imapmodelw + (m_imapmodelw - i - 1);
            m_fastmodel[inum] = 0;

        }
    }

    for (int j = 0; j < m_imapmodelh - invalidy; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum0 = i + (j + invalidy) * m_imapmodelw;
            int inum = i + j * m_imapmodelw;
            m_fastmodel[inum] = m_fastmodel[inum0];
        }
    }
    for (int j = 0; j < invalidy; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + m_imapmodelw * (m_imapmodelh - 1 - j);
            m_fastmodel[inum] = 0;
        }
    }
}
void Grid::ReSetModelGrid()
{
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = i + (j * m_imapmodelw);

            int ishownum = m_fastmodel[inum];
            if (ishownum != 1)
                m_fastmodel[inum] = 0;
        }
    }
}
void Grid::UnitGrid()
{
    int inum = 0;
    int ilinesum = 0;
    int upline = 0;
    int downline = 0;
    int rightline = 0;
    int leftline = 0;
    for (int i = 0; i < m_imapmodelh; i++)
    {
        ilinesum = 0;
        for (int j = 0; j < m_imapmodelw; j++)
        {
            inum = j + (i * m_imapmodelw);
            ilinesum = ilinesum + m_fastmodel[inum];

        }
        if (ilinesum > 0)
        {
            upline = i;
            break;

        }
    }

    for (int i = m_imapmodelh - 1; i >= 0; i--)
    {
        ilinesum = 0;
        for (int j = 0; j < m_imapmodelw; j++)
        {
            inum = j + (i * m_imapmodelw);
            ilinesum = ilinesum + m_fastmodel[inum];

        }
        if (ilinesum > 0)
        {
            downline = i;
            break;

        }
    }

    for (int i = 0; i < m_imapmodelw; i++)
    {
        ilinesum = 0;
        for (int j = 0; j < m_imapmodelh; j++)
        {
            inum = i + (j * m_imapmodelw);
            ilinesum = ilinesum + m_fastmodel[inum];
        }
        if (ilinesum > 0)
        {
            leftline = i;
            break;

        }
    }

    for (int i = m_imapmodelw - 1; i >= 0; i--)
    {
        ilinesum = 0;
        for (int j = 0; j < m_imapmodelh; j++)
        {
            inum = i + (j * m_imapmodelw);
            ilinesum = ilinesum + m_fastmodel[inum];
        }
        if (ilinesum > 0)
        {
            rightline = i;
            break;

        }
    }


    vector<int> oldmodel = m_fastmodel;
    m_fastmodel.clear();
    int inew_sum = 0;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            if (i >= leftline
                && i <= rightline
                && j >= upline
                && j <= downline)
            {
                inum = i + (j * m_imapmodelw);
                m_fastmodel.push_back(oldmodel[inum]);
                inew_sum = inew_sum + 1;
            }
        }
    }

    m_imapmodelw = rightline - leftline + 1;
    m_imapmodelh = downline - upline + 1;

    CentGrid(m_iunitw, m_iunith);
}
void Grid::EdgeGrid()
{
    int inum = 0;
    int ilinesum = 0;
    int upline = 0;
    int downline = 0;
    int rightline = 0;
    int leftline = 0;
    for (int i = 0; i < m_imapmodelh; i++)
    {
        ilinesum = 0;
        for (int j = 0; j < m_imapmodelw; j++)
        {
            inum = j + (i * m_imapmodelw);
            ilinesum = ilinesum + m_fastmodel[inum];

        }
        if (ilinesum > 0)
        {
            upline = i;
            break;

        }
    }

    for (int i = m_imapmodelh - 1; i >= 0; i--)
    {
        ilinesum = 0;
        for (int j = 0; j < m_imapmodelw; j++)
        {
            inum = j + (i * m_imapmodelw);
            ilinesum = ilinesum + m_fastmodel[inum];

        }
        if (ilinesum > 0)
        {
            downline = i;
            break;

        }
    }

    for (int i = 0; i < m_imapmodelw; i++)
    {
        ilinesum = 0;
        for (int j = 0; j < m_imapmodelh; j++)
        {
            inum = i + (j * m_imapmodelw);
            ilinesum = ilinesum + m_fastmodel[inum];
        }
        if (ilinesum > 0)
        {
            leftline = i;
            break;

        }
    }

    for (int i = m_imapmodelw - 1; i >= 0; i--)
    {
        ilinesum = 0;
        for (int j = 0; j < m_imapmodelh; j++)
        {
            inum = i + (j * m_imapmodelw);
            ilinesum = ilinesum + m_fastmodel[inum];
        }
        if (ilinesum > 0)
        {
            rightline = i;
            break;

        }
    }


    vector<int> oldmodel = m_fastmodel;
    m_fastmodel.clear();
    int inew_sum = 0;
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            if (i >= leftline
                && i <= rightline
                && j >= upline
                && j <= downline)
            {
                inum = i + (j * m_imapmodelw);
                m_fastmodel.push_back(oldmodel[inum]);
                inew_sum = inew_sum + 1;
            }
        }
    }

    m_imapmodelw = rightline - leftline + 1;
    m_imapmodelh = downline - upline + 1;

    CentGrid(m_iunitw, m_iunith);
}

void Grid::GridZoom(int iw, int ih)
{
    if (iw < m_imapmodelw
        && ih < m_imapmodelh)
    {
        double dratex = (iw * 1.0) / (m_imapmodelw * 1.0);
        double dratey = (ih * 1.0) / (m_imapmodelh * 1.0);
        int inum = 0;
        vector<int> newfastmodel;
        for (int i = 0; i < ih; i++)
        {
            for (int j = 0; j < iw; j++)
            {
                newfastmodel.push_back(0);
            }
        }
        for (int i = 0; i < m_imapmodelh; i++)
        {
            for (int j = 0; j < m_imapmodelw; j++)
            {
                int inewx = j * dratex;
                int inewy = i * dratey;
                int inewnum = iw * inewy + inewx;
                inum = m_imapmodelw * i + j;
                int itype = m_fastmodel[inum];
                if (0 == newfastmodel[inewnum]
                    && 0 != itype)
                    newfastmodel[inewnum] = itype;
            }
        }
        m_fastmodel.clear();
        m_fastmodel = newfastmodel;
        m_imapmodelh = ih;
        m_imapmodelw = iw;
    }
    else if (iw > m_imapmodelw
        && ih > m_imapmodelh)
    {

    }

}
void Grid::Grid2PattenModel_org(int icompgap)
{
    PointsShape pointsup;
    PointsShape pointsdown;
    PointsShape pointsright;
    PointsShape pointsleft;
    for (int i = 0; i < m_imapmodelw; i++)
    {
        for (int j = 0; j < m_imapmodelh; j++)
        {
            int inum = j * m_imapmodelw + i;
            int itype = m_fastmodel[inum];
            if (1 == itype)
            {
                gp_Pnt apoint(i, j,0);
                pointsup.addpoint(apoint);
                break;
            }
        }
    }
    for (int i = 0; i < m_imapmodelw; i++)
    {
        for (int j = m_imapmodelh - 1; j >= 0; j--)
        {
            int inum = j * m_imapmodelw + i;
            int itype = m_fastmodel[inum];
            if (1 == itype)
            {
                gp_Pnt apoint(i, j,0);
                pointsdown.addpoint(apoint);
                break;
            }
        }
    }
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = j * m_imapmodelw + i;
            int itype = m_fastmodel[inum];
            if (1 == itype)
            {
                gp_Pnt apoint(i, j,0);
                pointsright.addpoint(apoint);
                break;
            }
        }
    }
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = m_imapmodelw - 1; i >= 0; i--)
        {
            int inum = j * m_imapmodelw + i;
            int itype = m_fastmodel[inum];
            if (1 == itype)
            {
                gp_Pnt apoint(i, j,0);
                pointsleft.addpoint(apoint);
                break;
            }
        }
    }

    m_modelpoints.clear();
    pointsup.onepattern(icompgap, 12, m_modelpoints);
    pointsdown.onepattern(icompgap, 6, m_modelpoints);

    pointsright.onepattern(icompgap, 9, m_modelpoints);
    pointsleft.onepattern(icompgap, 3, m_modelpoints);


}

void Grid::Grid2PattenModel(int icompgap)
{
    PointsShape pointsup;
    PointsShape pointsdown;
    PointsShape pointsright;
    PointsShape pointsleft;
    for (int i = 0; i < m_imapmodelw; i++)
    {
        for (int j = 0; j < m_imapmodelh; j++)
        {
            int inum = j * m_imapmodelw + i;
            int itype = m_fastmodel[inum];
            if (1 == itype)
            {
                gp_Pnt apoint(i, j,0);
                pointsup.addpoint(apoint);
                break;
            }
        }
    }
    for (int i = 0; i < m_imapmodelw; i++)
    {
        for (int j = m_imapmodelh - 1; j >= 0; j--)
        {
            int inum = j * m_imapmodelw + i;
            int itype = m_fastmodel[inum];
            if (1 == itype)
            {
                gp_Pnt apoint(i, j,0);
                pointsdown.addpoint(apoint);
                break;
            }
        }
    }
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = 0; i < m_imapmodelw; i++)
        {
            int inum = j * m_imapmodelw + i;
            int itype = m_fastmodel[inum];
            if (1 == itype)
            {
                gp_Pnt apoint(i, j,0);
                pointsright.addpoint(apoint);
                break;
            }
        }
    }
    for (int j = 0; j < m_imapmodelh; j++)
    {
        for (int i = m_imapmodelw - 1; i >= 0; i--)
        {
            int inum = j * m_imapmodelw + i;
            int itype = m_fastmodel[inum];
            if (1 == itype)
            {
                gp_Pnt apoint(i, j,0);
                pointsleft.addpoint(apoint);
                break;
            }
        }
    }

    m_modelpoints.clear();
    pointsup.onepattern(icompgap, 12, m_modelpoints);
    pointsdown.onepattern(icompgap, 6, m_modelpoints);

    pointsright.onepattern(icompgap, 9, m_modelpoints);
    pointsleft.onepattern(icompgap, 3, m_modelpoints);

    gp_Rectangle arect1 = m_modelpoints.boundingRect();
    m_modelpoints.Move(-arect1.TopLeft().X(), -arect1.TopLeft().Y());

}
void Grid::SetFastModel(vector<int>& fastmodel)
{
    m_fastmodel = fastmodel;
}
void Grid::SetModelWH(int iw, int ih)
{
    m_imapmodelw = iw;
    m_imapmodelh = ih;
}
string Grid::GetGridString()
{
    std::ostringstream stream;
    stream << m_imapmodelw << "x" << m_imapmodelh;
    std::string astr = stream.str();
    return astr;
}

void Grid::GridFastModel(Grid* pgrid)
{
    m_fastmodel = pgrid->getfastmodel();
}

bool Grid::GridCompare(Grid* pgrid)
{
    if (0 == pgrid)
        return false;
    const int isize = static_cast<int>(m_fastmodel.size());
    const int isize2 = static_cast<int>(pgrid->getfastmodel().size());
    if (isize != isize2)
        return false;

    for (int i = 0; i < isize; i++)
    {
        if (m_fastmodel[i] != pgrid->getfastmodel()[i])
            return false;
    }
    return true;
}
void Grid::SetUnit(int iw, int ih)
{
    m_iunith = ih;
    m_iunitw = iw;
}
gp_Rectangle Grid::CentRect_Condition(gp_Rectangle& rect, int imax)
{
    const int ix = static_cast<int>(rect.TopLeft().X());
    const int iy = static_cast<int>(rect.TopLeft().Y());
    const int iw = static_cast<int>(rect.Width());
    const int ih = static_cast<int>(rect.Height());
    int igapw = (m_iunitw - iw) / 2;
    int igaph = (m_iunith - ih) / 2;
    if (igapw <= imax && igaph <= imax)
    {
        int ibeginx = ix - igapw;
        int ibeginy = iy - igaph;

        return gp_Rectangle(gp_Pnt(ibeginx, ibeginy,0), m_iunitw, m_iunith);

    }
    else
    {
        int ibeginx = ix - imax;
        int ibeginy = iy - imax;
        int iw1 = iw + imax * 2;
        int ih1 = ih + imax * 2;
        return gp_Rectangle(gp_Pnt(ibeginx, ibeginy,0), iw1, ih1);
    }

}
gp_Rectangle Grid::CentRect(gp_Rectangle& rect)
{
    const int ix = static_cast<int>(rect.TopLeft().X());
    const int iy = static_cast<int>(rect.TopLeft().Y());
    const int iw = static_cast<int>(rect.Width());
    const int ih = static_cast<int>(rect.Height());
    int igapw = (m_iunitw - iw) / 2;
    int igaph = (m_iunith - ih) / 2;
    int ibeginx = ix - igapw;
    int ibeginy = iy - igaph;
    return gp_Rectangle(gp_Pnt(ibeginx, ibeginy,0), m_iunitw, m_iunith);
}
void Grid::CentGrid(int iw, int ih)
{

    int ibeginx = 0;
    int iendx = 0;
    int ibeginy = 0;
    int iendy = 0;

    if (m_fastmodel.size() <= 0)
        return;

    ibeginx = (iw - m_imapmodelw) / 2;
    ibeginx = ibeginx >= 0 ? ibeginx : 0;
    iendx = ibeginx + m_imapmodelw;

    ibeginy = (ih - m_imapmodelh) / 2;
    ibeginy = ibeginy >= 0 ? ibeginy : 0;
    iendy = ibeginy + m_imapmodelh;


    vector<int> oldmodel = m_fastmodel;

    m_fastmodel.clear();
    int inew_sum = 0;
    for (int j = 0; j < ih; j++)
    {
        for (int i = 0; i < iw; i++)
        {
            if (i >= ibeginx && i < iendx &&
                j >= ibeginy && j < iendy)
            {
                m_fastmodel.push_back(oldmodel[inew_sum]);
                inew_sum = inew_sum + 1;
            }
            else
                m_fastmodel.push_back(0);
        }
    }

    m_imapmodelw = iw;
    m_imapmodelh = ih;

}
void Grid::savemapmodel(const char* pchar)
{
    int isize = m_gridT.size();
    if (isize <= 0)
        return;
    FILE* rf = fopen(pchar, "w+");
    if (rf == NULL)
        return;
    rewind(rf);
    int inum = 0;
    for (int ish = 0; ish < m_imapmodelh; ish++)
    {
        for (int isw = 0; isw < m_imapmodelw; isw++)
        {
            if (isw != 0)
                fprintf(rf, ",");
            fprintf(rf, "%d", m_fastmodel[inum]);

            inum++;

        }
        fprintf(rf, "\r\n");
    }
    fclose(rf);
}
void Grid::loadmapmodel(const char* pchar)
{
    
}
void Grid::ReGrid(int iw, int ih)
{
    vector<int> oldmodel = m_fastmodel;
    m_fastmodel.clear();
    for (int j = 0; j < ih; j++)
    {
        for (int i = 0; i < iw; i++)
        {
            if (i < m_imapmodelw
                && j < m_imapmodelh)
            {
                int inum = i + (j * m_imapmodelw);
                m_fastmodel.push_back(oldmodel[inum]);
            }
            else
                m_fastmodel.push_back(0);
        }
    }
    m_imapmodelh = ih;
    m_imapmodelw = iw;

}
vector<int>& Grid::getfastmodel()
{
    return m_fastmodel;
}
PointsShape& Grid::getpatmodel()
{
    return m_modelpoints;
}
