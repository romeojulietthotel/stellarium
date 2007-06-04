/*
 * Stellarium
 * Copyright (C) 2002 Fabien Chereau
 *
 * The big star catalogue extension to Stellarium:
 * Author and Copyright: Johannes Gajdosik, 2006
 *  (I will move these parts to new files,
 *   so that the Copyright is more clear).
 *
 * MSB unpacking of gcc bitfields by Nigel Kerr
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

// class used to manage groups of Stars

#include <string>
#include <list>
#include <config.h>

#include "Projector.hpp"
#include "StarMgr.hpp"
#include "StelObject.hpp"
#include "STexture.hpp"
#include "Navigator.hpp"
#include "StelUtils.hpp"
#include "ToneReproducer.hpp"
#include "Translator.hpp"
#include "GeodesicGrid.hpp"
#include "LoadingBar.hpp"
#include "Translator.hpp"
#include "StelApp.hpp"
#include "StelTextureMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelFontMgr.hpp"
#include "StelLocaleMgr.hpp"
#include "StelSkyCultureMgr.hpp"
#include "StelFileMgr.hpp"
#include "InitParser.hpp"
#include "bytes.h"
#include "InitParser.hpp"

typedef int Int32;
typedef unsigned int Uint32;
typedef short int Int16;
typedef unsigned short int Uint16;


#define NR_OF_HIP 120416

class StringArray {
public:
  StringArray(void) : array(0),size(0) {}
  ~StringArray(void) {clear();}
  void clear(void) {if (array) {delete[] array;array = 0;} size= 0;}
  int getSize(void) const {return size;}
  string operator[](int i) const {return ((0<=i && i<size) ? array[i] : "");}
  void initFromFile(const char *file_name);
private:
  string *array;
  int size;
};

void StringArray::initFromFile(const char *file_name) {
  clear();
  list<string> list;
  FILE *f = fopen(file_name,"r");
  if (f) {
    char line[256];
    while (fgets(line, sizeof(line), f)) {
      string s = line;
      // remove newline
      s.erase(s.length()-1,1);
      list.push_back(s);
      size++;
    }
    fclose(f);
  }
  if (size > 0) {
    array = new string[size];
    assert(array!=0);
    for (int i=0;i<size;i++) {
      array[i] = list.front();
      list.pop_front();
    }
  }
}

static StringArray spectral_array;
static StringArray component_array;

static string convertToSpectralType(int index) {
  if (index < 0 || index >= spectral_array.getSize()) {
    cout << "convertToSpectralType: bad index: " << index
         << ", max: " << spectral_array.getSize() << endl;
    return "";
  }
  return spectral_array[index];
}

static string convertToComponentIds(int index) {
  if (index < 0 || index >= component_array.getSize()) {
    cout << "convertToComponentIds: bad index: " << index
         << ", max: " << component_array.getSize() << endl;
    return "";
  }
  return component_array[index];
}






struct HipIndexStruct;

template <class Star> struct SpecialZoneArray;
template <class Star> struct SpecialZoneData;


struct ZoneData { // a single Triangle
    // no virtual functions!
  int getNrOfStars(void) const {return size;}
  Vec3d center;
  Vec3d axis0;
  Vec3d axis1;
  int size;
  void *stars;
};


#ifdef WORDS_BIGENDIAN

// MSB UnpackBits and UnpackUBits really implemented the same as LSB (below)
// but we use a local char[4] to swap the bytes about before the cast to int
// or unsigned int.

static
int UnpackBits(const char *addr,int bits_begin,const int bits_size) {
	while (bits_begin >= 8) {
		bits_begin -= 8;
		addr++;
	}

	char swapper[4];
	swapper[3] = addr[0];
	swapper[2] = addr[1];
	swapper[1] = addr[2];
	swapper[0] = addr[3];
	
	int rval = ((const int*)(&(swapper[0])))[0];
	
	const int bits_end = bits_begin + bits_size;
	if (bits_end <= 32) {
		if (bits_end < 32) rval <<= (32-bits_end);
		if (bits_size < 32) rval >>= (32-bits_size);
		
	} else {
		rval >>= bits_begin;
		const int mask = (1<<(32-bits_begin))-1;
		rval &= mask;
		swapper[3] = addr[4];
		swapper[2] = addr[5];
		swapper[1] = addr[6];
		swapper[0] = addr[7];
		
		int rval_hi = ((const int*)(&(swapper[0])))[0];
		rval_hi <<= (64-bits_end);
		rval_hi >>= (32-bits_size);
		rval_hi &= ~mask;
		rval |= rval_hi;
	}
	return rval;
}

static
unsigned int UnpackUBits(const char *addr,int bits_begin,const int bits_size) {
	while (bits_begin >= 8) {
		bits_begin -= 8;
		addr++;
	}

	char swapper[4];
	swapper[3] = addr[0];
	swapper[2] = addr[1];
	swapper[1] = addr[2];
	swapper[0] = addr[3];
	
	unsigned int rval = ((const unsigned int*)(&(swapper[0])))[0];
	
	const int bits_end = bits_begin + bits_size;
	if (bits_end <= 32) {
		if (bits_begin > 0) rval >>= bits_begin;
	} else {
		rval >>= bits_begin;
		const int mask = (((unsigned int)1)<<(32-bits_begin))-1;
		rval &= mask;
		swapper[3] = addr[4];
		swapper[2] = addr[5];
		swapper[1] = addr[6];
		swapper[0] = addr[7];
		
		unsigned int rval_hi = ((const unsigned int*)(&(swapper[0])))[0];
		rval_hi <<= (32-bits_begin);
		rval_hi &= ~mask;
		rval |= rval_hi;
	}
	if (bits_size < 32) rval &= ((((unsigned int)1)<<bits_size)-1);
	return rval;
}

#else

static
int UnpackBits(const char *addr,int bits_begin,const int bits_size) {
  while (bits_begin >= 8) {
    bits_begin -= 8;
    addr++;
  }
  int rval = ((const int*)addr)[0];
  const int bits_end = bits_begin + bits_size;
  if (bits_end <= 32) {
    if (bits_end < 32) rval <<= (32-bits_end);
    if (bits_size < 32) rval >>= (32-bits_size);
  } else {
    rval >>= bits_begin;
    const int mask = (1<<(32-bits_begin))-1;
    rval &= mask;
    int rval_hi = ((const int*)addr)[1];
    rval_hi <<= (64-bits_end);
    rval_hi >>= (32-bits_size);
    rval_hi &= ~mask;
    rval |= rval_hi;
  }
  return rval;
}

static
unsigned int UnpackUBits(const char *addr,int bits_begin,const int bits_size) {
  while (bits_begin >= 8) {
    bits_begin -= 8;
    addr++;
  }
  unsigned int rval = ((const unsigned int*)addr)[0];
  const int bits_end = bits_begin + bits_size;
  if (bits_end <= 32) {
    if (bits_begin > 0) rval >>= bits_begin;
  } else {
    rval >>= bits_begin;
    const int mask = (((unsigned int)1)<<(32-bits_begin))-1;
    rval &= mask;
    unsigned int rval_hi = ((const unsigned int*)addr)[1];
    rval_hi <<= (32-bits_begin);
    rval_hi &= ~mask;
    rval |= rval_hi;
  }
  if (bits_size < 32) rval &= ((((unsigned int)1)<<bits_size)-1);
  return rval;
}

#endif



struct Star3 {  // 6 byte
  int x0:18;
  int x1:18;
  unsigned int b_v:7;
  unsigned int mag:5;
  enum {max_pos_val=((1<<17)-1)};
  StelObjectP createStelObject(const SpecialZoneArray<Star3> *a,
                               const SpecialZoneData<Star3> *z) const;
  Vec3d getJ2000Pos(const ZoneData *z,double) const {
    Vec3d pos = z->center + (double)(x0)*z->axis0 + (double)(x1)*z->axis1;
    pos.normalize();
    return pos;
  }
  float getBV(void) const {return b_v*(4.0/127.0)-0.5;}
  wstring getNameI18n(void) const {return L"";}
  void repack(void);
//  void print(void);
} __attribute__ ((__packed__)) ;

void Star3::repack(void) {
  const int _x0  = UnpackBits((const char*)this, 0,18);
  const int _x1  = UnpackBits((const char*)this,18,18);
  const unsigned int _b_v = UnpackUBits((const char*)this,36, 7);
  const unsigned int _mag = UnpackUBits((const char*)this,43, 5);
  x0 = _x0;
  x1 = _x1;
  b_v = _b_v;
  mag = _mag;
}

//void Star3::print(void) {
//  cout << "x0: " << x0
//       << ", x1: " << x1
//       << ", b_v: " << b_v
//       << ", mag: " << mag
//       << endl;
//}


struct Star2 {  // 10 byte
  int x0:20;
  int x1:20;
  int dx0:14;
  int dx1:14;
  unsigned int b_v:7;
  unsigned int mag:5;
  enum {max_pos_val=((1<<19)-1)};
  StelObjectP createStelObject(const SpecialZoneArray<Star2> *a,
                               const SpecialZoneData<Star2> *z) const;
  Vec3d getJ2000Pos(const ZoneData *z,double movement_factor) const {
    Vec3d pos = z->center
              + (x0+movement_factor*dx0)*z->axis0
              + (x1+movement_factor*dx1)*z->axis1;
    pos.normalize();
    return pos;
  }
  float getBV(void) const {return b_v*(4.0/127.0)-0.5;}
  wstring getNameI18n(void) const {return L"";}
  void repack(void);
//  void print(void);
} __attribute__ ((__packed__));

void Star2::repack(void) {
  const int _x0  = UnpackBits((const char*)this, 0,20);
  const int _x1  = UnpackBits((const char*)this,20,20);
  const int _dx0 = UnpackBits((const char*)this,40,14);
  const int _dx1 = UnpackBits((const char*)this,54,14);
  const unsigned int _b_v = UnpackUBits((const char*)this,68, 7);
  const unsigned int _mag = UnpackUBits((const char*)this,75, 5);
  x0 = _x0;
  x1 = _x1;
  dx0 = _dx0;
  dx1 = _dx1;
  b_v = _b_v;
  mag = _mag;
}

//void Star2::print(void) {
//  cout << "x0: " << x0
//       << ", x1: " << x1
//       << ", dx0: " << dx0
//       << ", dx1: " << dx1
//       << ", b_v: " << b_v
//       << ", mag: " << mag
//       << endl;
//}



struct Star1 { // 28 byte
  int hip:24;                  // 17 bits needed
  unsigned char component_ids; //  5 bits needed
  Int32 x0;                    // 32 bits needed
  Int32 x1;                    // 32 bits needed
  unsigned char b_v;           //  7 bits needed
  unsigned char mag;           //  8 bits needed
  Uint16 sp_int;               // 14 bits needed
  Int32 dx0,dx1,plx;
  enum {max_pos_val=0x7FFFFFFF};
  StelObjectP createStelObject(const SpecialZoneArray<Star1> *a,
                               const SpecialZoneData<Star1> *z) const;
  Vec3d getJ2000Pos(const ZoneData *z,double movement_factor) const {
    Vec3d pos = z->center
              + (x0+movement_factor*dx0)*z->axis0
              + (x1+movement_factor*dx1)*z->axis1;
    pos.normalize();
    return pos;
  }
  float getBV(void) const {return b_v*(4.0/127.0)-0.5;}
  wstring getNameI18n(void) const {
    if (hip) {
      const wstring commonNameI18 = StarMgr::getCommonName(hip);
      if (!commonNameI18.empty()) return commonNameI18;
      if (StarMgr::getFlagSciNames()) {
        const wstring sciName = StarMgr::getSciName(hip);
        if (!sciName.empty()) return sciName;
      }
      return L"HP " + StelUtils::intToWstring(hip);
    }
    return L"";
  }
  void repack(void);
//  void print(void);
} __attribute__ ((__packed__));

void Star1::repack(void) {
  const int _hip  = UnpackBits((const char*)this, 0,24);
  const unsigned int _cids = UnpackUBits((const char*)this,24, 8);
  const int _x0  = UnpackBits((const char*)this,32,32);
  const int _x1  = UnpackBits((const char*)this,64,32);
  const unsigned int _b_v = UnpackUBits((const char*)this, 96, 8);
  const unsigned int _mag = UnpackUBits((const char*)this,104, 8);
  const unsigned int _sp_int = UnpackUBits((const char*)this,112,16);
  const int _dx0 = UnpackBits((const char*)this,128,32);
  const int _dx1 = UnpackBits((const char*)this,160,32);
  const int _plx = UnpackBits((const char*)this,192,32);
  hip = _hip;
  component_ids = _cids;
  x0 = _x0;
  x1 = _x1;
  b_v = _b_v;
  mag = _mag;
  sp_int = _sp_int;
  dx0 = _dx0;
  dx1 = _dx1;
  plx = _plx;
}

//void Star1::print(void) {
//  cout << "hip: " << hip
//       << ", component_ids: " << ((unsigned int)component_ids)
//       << ", x0: " << x0
//       << ", x1: " << x1
//       << ", b_v: " << ((unsigned int)b_v)
//       << ", mag: " << ((unsigned int)mag)
//       << ", sp_int: " << sp_int
//       << ", dx0: " << dx0
//       << ", dx1: " << dx1
//       << ", plx: " << plx
//       << endl;
//}


template <class Star>
struct SpecialZoneData : public ZoneData {
  StelObjectP createStelObject(const Star *s) const
    {return s->createStelObject(*this);}
  Star *getStars(void) const {return reinterpret_cast<Star*>(stars);}
     // array of stars in this zone
};

class ZoneArray {  // contains all zones of a given level
public:
  static ZoneArray *create(const StarMgr &hip_star_mgr,const char *fname,
                           LoadingBar &lb);
  virtual ~ZoneArray(void) {nr_of_zones = 0;}
  int getNrOfStars(void) const {return nr_of_stars;}
  virtual void updateHipIndex(HipIndexStruct hip_index[]) const {}
  virtual void searchAround(int index,const Vec3d &v,double cos_lim_fov,
                            vector<StelObjectP > &result) = 0;
  virtual void draw(int index,bool is_inside,bool draw_point,
                    const float *rmag_table, Projector *prj,
                    unsigned int max_mag_star_name,float names_brightness,
                    SFont *starFont,
                    STextureSP starTexture) const = 0;
  bool isInitialized(void) const {return (nr_of_zones>0);}
  void initTriangle(int index,
                    const Vec3d &c0,
                    const Vec3d &c1,
                    const Vec3d &c2);
  virtual void scaleAxis(void) = 0;
  const int level;
  const int mag_min;
  const int mag_range;
  const int mag_steps;
  double star_position_scale;
protected:
  ZoneArray(const StarMgr &hip_star_mgr,int level,
            int mag_min,int mag_range,int mag_steps);
  const StarMgr &hip_star_mgr;
  int nr_of_zones;
  int nr_of_stars;
  ZoneData *zones;
};

template<class Star>
class SpecialZoneArray : public ZoneArray {
public:
  SpecialZoneArray(FILE *f,LoadingBar &lb,
                   const StarMgr &hip_star_mgr,int level,
                   int mag_min,int mag_range,int mag_steps);
  ~SpecialZoneArray(void) {
    if (stars) {delete[] stars;stars = NULL;}
    if (zones) {delete[] getZones();zones = NULL;}
    nr_of_zones = 0;
    nr_of_stars = 0;
  }
protected:
  SpecialZoneData<Star> *getZones(void) const
    {return static_cast<SpecialZoneData<Star>*>(zones);}
  Star *stars;
private:
  void scaleAxis(void);
  void searchAround(int index,const Vec3d &v,double cos_lim_fov,
                    vector<StelObjectP > &result);
  void draw(int index,bool is_inside,bool draw_point,
            const float *rmag_table, Projector *prj,
            unsigned int max_mag_star_name,float names_brightness,
            SFont *starFont,STextureSP starTexture) const;
};

struct HipIndexStruct {
  const SpecialZoneArray<Star1> *a;
  const SpecialZoneData<Star1> *z;
  const Star1 *s;
};

class ZoneArray1 : public SpecialZoneArray<Star1> {
public:
  ZoneArray1(FILE *f,LoadingBar &lb,const StarMgr &hip_star_mgr,int level,
             int mag_min,int mag_range,int mag_steps)
    : SpecialZoneArray<Star1>(f,lb,hip_star_mgr,level,
                              mag_min,mag_range,mag_steps) {}
private:
  void updateHipIndex(HipIndexStruct hip_index[]) const;
};




void StarMgr::initTriangle(int lev,int index,
                              const Vec3d &c0,
                              const Vec3d &c1,
                              const Vec3d &c2) {
  ZoneArrayMap::const_iterator it(zone_arrays.find(lev));
  if (it!=zone_arrays.end()) it->second->initTriangle(index,c0,c1,c2);
}


static const Vec3d north(0,0,1);

void ZoneArray::initTriangle(int index,
                             const Vec3d &c0,
                             const Vec3d &c1,
                             const Vec3d &c2) {
    // initialize center,axis0,axis1:
  ZoneData &z(zones[index]);
  z.center = c0+c1+c2;
  z.center.normalize();
  z.axis0 = north ^ z.center;
  z.axis0.normalize();
  z.axis1 = z.center ^ z.axis0;
    // initialize star_position_scale:
  double mu0,mu1,f,h;
  mu0 = (c0-z.center)*z.axis0;
  mu1 = (c0-z.center)*z.axis1;
  f = 1.0/sqrt(1.0-mu0*mu0-mu1*mu1);
  h = fabs(mu0)*f;
  if (star_position_scale < h) star_position_scale = h;
  h = fabs(mu1)*f;
  if (star_position_scale < h) star_position_scale = h;
  mu0 = (c1-z.center)*z.axis0;
  mu1 = (c1-z.center)*z.axis1;
  f = 1.0/sqrt(1.0-mu0*mu0-mu1*mu1);
  h = fabs(mu0)*f;
  if (star_position_scale < h) star_position_scale = h;
  h = fabs(mu1)*f;
  if (star_position_scale < h) star_position_scale = h;
  mu0 = (c2-z.center)*z.axis0;
  mu1 = (c2-z.center)*z.axis1;
  f = 1.0/sqrt(1.0-mu0*mu0-mu1*mu1);
  h = fabs(mu0)*f;
  if (star_position_scale < h) star_position_scale = h;
  h = fabs(mu1)*f;
  if (star_position_scale < h) star_position_scale = h;
}

template<class Star>
void SpecialZoneArray<Star>::scaleAxis(void) {
  star_position_scale /= Star::max_pos_val;
  for (ZoneData *z=zones+(nr_of_zones-1);z>=zones;z--) {
    z->axis0 *= star_position_scale;
    z->axis1 *= star_position_scale;
  }
}





static
const Vec3f color_table[128] = {
  Vec3f(0.587877,0.755546,1.000000),
  Vec3f(0.609856,0.750638,1.000000),
  Vec3f(0.624467,0.760192,1.000000),
  Vec3f(0.639299,0.769855,1.000000),
  Vec3f(0.654376,0.779633,1.000000),
  Vec3f(0.669710,0.789527,1.000000),
  Vec3f(0.685325,0.799546,1.000000),
  Vec3f(0.701229,0.809688,1.000000),
  Vec3f(0.717450,0.819968,1.000000),
  Vec3f(0.733991,0.830383,1.000000),
  Vec3f(0.750857,0.840932,1.000000),
  Vec3f(0.768091,0.851637,1.000000),
  Vec3f(0.785664,0.862478,1.000000),
  Vec3f(0.803625,0.873482,1.000000),
  Vec3f(0.821969,0.884643,1.000000),
  Vec3f(0.840709,0.895965,1.000000),
  Vec3f(0.859873,0.907464,1.000000),
  Vec3f(0.879449,0.919128,1.000000),
  Vec3f(0.899436,0.930956,1.000000),
  Vec3f(0.919907,0.942988,1.000000),
  Vec3f(0.940830,0.955203,1.000000),
  Vec3f(0.962231,0.967612,1.000000),
  Vec3f(0.984110,0.980215,1.000000),
  Vec3f(1.000000,0.986617,0.993561),
  Vec3f(1.000000,0.977266,0.971387),
  Vec3f(1.000000,0.967997,0.949602),
  Vec3f(1.000000,0.958816,0.928210),
  Vec3f(1.000000,0.949714,0.907186),
  Vec3f(1.000000,0.940708,0.886561),
  Vec3f(1.000000,0.931787,0.866303),
  Vec3f(1.000000,0.922929,0.846357),
  Vec3f(1.000000,0.914163,0.826784),
  Vec3f(1.000000,0.905497,0.807593),
  Vec3f(1.000000,0.896884,0.788676),
  Vec3f(1.000000,0.888389,0.770168),
  Vec3f(1.000000,0.879953,0.751936),
  Vec3f(1.000000,0.871582,0.733989),
  Vec3f(1.000000,0.863309,0.716392),
  Vec3f(1.000000,0.855110,0.699088),
  Vec3f(1.000000,0.846985,0.682070),
  Vec3f(1.000000,0.838928,0.665326),
  Vec3f(1.000000,0.830965,0.648902),
  Vec3f(1.000000,0.823056,0.632710),
  Vec3f(1.000000,0.815254,0.616856),
  Vec3f(1.000000,0.807515,0.601243),
  Vec3f(1.000000,0.799820,0.585831),
  Vec3f(1.000000,0.792222,0.570724),
  Vec3f(1.000000,0.784675,0.555822),
  Vec3f(1.000000,0.777212,0.541190),
  Vec3f(1.000000,0.769821,0.526797),
  Vec3f(1.000000,0.762496,0.512628),
  Vec3f(1.000000,0.755229,0.498664),
  Vec3f(1.000000,0.748032,0.484926),
  Vec3f(1.000000,0.740897,0.471392),
  Vec3f(1.000000,0.733811,0.458036),
  Vec3f(1.000000,0.726810,0.444919),
  Vec3f(1.000000,0.719856,0.431970),
  Vec3f(1.000000,0.712983,0.419247),
  Vec3f(1.000000,0.706154,0.406675),
  Vec3f(1.000000,0.699375,0.394265),
  Vec3f(1.000000,0.692681,0.382075),
  Vec3f(1.000000,0.686003,0.369976),
  Vec3f(1.000000,0.679428,0.358120),
  Vec3f(1.000000,0.672882,0.346373),
  Vec3f(1.000000,0.666372,0.334740),
  Vec3f(1.000000,0.659933,0.323281),
  Vec3f(1.000000,0.653572,0.312004),
  Vec3f(1.000000,0.647237,0.300812),
  Vec3f(1.000000,0.640934,0.289709),
  Vec3f(1.000000,0.634698,0.278755),
  Vec3f(1.000000,0.628536,0.267954),
  Vec3f(1.000000,0.622390,0.257200),
  Vec3f(1.000000,0.616298,0.246551),
  Vec3f(1.000000,0.610230,0.235952),
  Vec3f(1.000000,0.604259,0.225522),
  Vec3f(1.000000,0.598288,0.215083),
  Vec3f(1.000000,0.592391,0.204756),
  Vec3f(1.000000,0.586501,0.194416),
  Vec3f(1.000000,0.580657,0.184120),
  Vec3f(1.000000,0.574901,0.173930),
  Vec3f(1.000000,0.569127,0.163645),
  Vec3f(1.000000,0.563449,0.153455),
  Vec3f(1.000000,0.557758,0.143147),
  Vec3f(1.000000,0.552134,0.132843),
  Vec3f(1.000000,0.546541,0.122458),
  Vec3f(1.000000,0.540984,0.111966),
  Vec3f(1.000000,0.535464,0.101340),
  Vec3f(1.000000,0.529985,0.090543),
  Vec3f(1.000000,0.524551,0.079292),
  Vec3f(1.000000,0.519122,0.068489),
  Vec3f(1.000000,0.513743,0.058236),
  Vec3f(1.000000,0.508417,0.048515),
  Vec3f(1.000000,0.503104,0.039232),
  Vec3f(1.000000,0.497805,0.030373),
  Vec3f(1.000000,0.492557,0.021982),
  Vec3f(1.000000,0.487338,0.014007),
  Vec3f(1.000000,0.482141,0.006417),
  Vec3f(1.000000,0.477114,0.000000),
  Vec3f(1.000000,0.473268,0.000000),
  Vec3f(1.000000,0.469419,0.000000),
  Vec3f(1.000000,0.465552,0.000000),
  Vec3f(1.000000,0.461707,0.000000),
  Vec3f(1.000000,0.457846,0.000000),
  Vec3f(1.000000,0.453993,0.000000),
  Vec3f(1.000000,0.450129,0.000000),
  Vec3f(1.000000,0.446276,0.000000),
  Vec3f(1.000000,0.442415,0.000000),
  Vec3f(1.000000,0.438549,0.000000),
  Vec3f(1.000000,0.434702,0.000000),
  Vec3f(1.000000,0.430853,0.000000),
  Vec3f(1.000000,0.426981,0.000000),
  Vec3f(1.000000,0.423134,0.000000),
  Vec3f(1.000000,0.419268,0.000000),
  Vec3f(1.000000,0.415431,0.000000),
  Vec3f(1.000000,0.411577,0.000000),
  Vec3f(1.000000,0.407733,0.000000),
  Vec3f(1.000000,0.403874,0.000000),
  Vec3f(1.000000,0.400029,0.000000),
  Vec3f(1.000000,0.396172,0.000000),
  Vec3f(1.000000,0.392331,0.000000),
  Vec3f(1.000000,0.388509,0.000000),
  Vec3f(1.000000,0.384653,0.000000),
  Vec3f(1.000000,0.380818,0.000000),
  Vec3f(1.000000,0.376979,0.000000),
  Vec3f(1.000000,0.373166,0.000000),
  Vec3f(1.000000,0.369322,0.000000),
  Vec3f(1.000000,0.365506,0.000000),
  Vec3f(1.000000,0.361692,0.000000),
};



class StarWrapperBase : public StelObject {
protected:
  StarWrapperBase(void) : ref_count(0) {}
  virtual ~StarWrapperBase(void) {}
  std::string getType(void) const {return "Star";}

  string getEnglishName(void) const {return "";}
  wstring getNameI18n(void) const = 0;
  wstring getInfoString(const Navigator *nav) const;
  wstring getShortInfoString(const Navigator *nav) const;
  virtual float getBV(void) const = 0;
private:
  int ref_count;
  void retain(void) {assert(ref_count>=0);ref_count++;}
  void release(void) {assert(ref_count>0);if (--ref_count==0) delete this;}
};

wstring StarWrapperBase::getInfoString(const Navigator *nav) const {
  const Vec3d j2000_pos = getObsJ2000Pos(nav);
  double dec_j2000, ra_j2000;
  StelUtils::rect_to_sphe(&ra_j2000,&dec_j2000,j2000_pos);
  const Vec3d equatorial_pos = nav->j2000_to_earth_equ(j2000_pos);
  double dec_equ, ra_equ;
  StelUtils::rect_to_sphe(&ra_equ,&dec_equ,equatorial_pos);
  wostringstream oss;
  oss.setf(ios::fixed);
  oss.precision(2);
  oss << _("Magnitude: ") << getMagnitude(nav)
      << " B-V: " << getBV()
      << endl;
  oss << _("J2000") << L" " << _("RA/DE: ")
      << StelUtils::printAngleHMS(ra_j2000,true)
      << L"/" << StelUtils::printAngleDMS(dec_j2000,true) << endl;
  oss << _("Equ of date") << L" " << _("RA/DE: ")
      << StelUtils::printAngleHMS(ra_equ)
      << L"/" << StelUtils::printAngleDMS(dec_equ) << endl;

    // calculate alt az
  double az,alt;
  StelUtils::rect_to_sphe(&az,&alt,nav->earth_equ_to_local(equatorial_pos));
  az = 3*M_PI - az;  // N is zero, E is 90 degrees
  if(az > M_PI*2) az -= M_PI*2;    
  oss << _("Az/Alt: ") << StelUtils::printAngleDMS(az)
      << L"/" << StelUtils::printAngleDMS(alt) << endl;
  oss.precision(2);

  return oss.str();
}

wstring StarWrapperBase::getShortInfoString(const Navigator *nav) const {
	wostringstream oss;
	oss.setf(ios::fixed);
	oss.precision(2);
	oss << _("Magnitude: ") << getMagnitude(nav);

	return oss.str();
}


static const double d2000 = 2451545.0;
static double current_JDay = d2000;

template <class Star>
class StarWrapper : public StarWrapperBase {
protected:
  StarWrapper(const SpecialZoneArray<Star> *a,
              const SpecialZoneData<Star> *z,
              const Star *s) : a(a),z(z),s(s) {}
  Vec3d getObsJ2000Pos(const Navigator *nav) const {
    if (nav) current_JDay = nav->getJDay();
    return s->getJ2000Pos(z,
                  (M_PI/180)*(0.0001/3600)
                   * ((current_JDay-d2000)/365.25)
                   / a->star_position_scale
                 );
  }
  Vec3d getEarthEquatorialPos(const Navigator *nav) const
    {return nav->j2000_to_earth_equ(getObsJ2000Pos(nav));}
  Vec3f getInfoColor(void) const {return color_table[s->b_v];}
  float getMagnitude(const Navigator *nav) const
    {return 0.001f*a->mag_min + s->mag*(0.001f*a->mag_range)/a->mag_steps;}
  float getSelectPriority(const Navigator *nav) const
  {
  	return getMagnitude(nav);
  }
  float getBV(void) const {return s->getBV();}
  string getEnglishName(void) const {
    return "";
  }
  wstring getNameI18n(void) const {
    return s->getNameI18n();
  }
protected:
  const SpecialZoneArray<Star> *const a;
  const SpecialZoneData<Star> *const z;
  const Star *const s;
};

class StarWrapper1 : public StarWrapper<Star1> {
public:
  StarWrapper1(const SpecialZoneArray<Star1> *a,
               const SpecialZoneData<Star1> *z,
               const Star1 *s) : StarWrapper<Star1>(a,z,s) {}
  wstring getInfoString(const Navigator *nav) const;
  wstring getShortInfoString(const Navigator *nav) const;
  string getEnglishName(void) const;
};

string StarWrapper1::getEnglishName(void) const {
  if (s->hip) {
    char buff[64];
    sprintf(buff,"HP %d",s->hip);
    return buff;
  }
  return StarWrapperBase::getEnglishName();
}

wstring StarWrapper1::getInfoString(const Navigator *nav) const {
  const Vec3d j2000_pos = getObsJ2000Pos(nav);
  double dec_j2000, ra_j2000;
  StelUtils::rect_to_sphe(&ra_j2000,&dec_j2000,j2000_pos);
  const Vec3d equatorial_pos = nav->j2000_to_earth_equ(j2000_pos);
  double dec_equ, ra_equ;
  StelUtils::rect_to_sphe(&ra_equ,&dec_equ,equatorial_pos);
  wostringstream oss;
  if (s->hip) {
    const wstring commonNameI18 = StarMgr::getCommonName(s->hip);
    const wstring sciName = StarMgr::getSciName(s->hip);
    if (commonNameI18!=L"" || sciName!=L"") {
      oss << commonNameI18 << (commonNameI18 == L"" ? L"" : L" ");
      if (commonNameI18!=L"" && sciName!=L"") oss << L"(";
      oss << wstring(sciName==L"" ? L"" : sciName);
      if (commonNameI18!=L"" && sciName!=L"") oss << L")";
      oss << endl;
    }
    oss << L"HP " << s->hip;
    if (s->component_ids) {
      oss << L" "
          << convertToComponentIds(s->component_ids).c_str();
    }
    oss << endl;
  }

  oss.setf(ios::fixed);
  oss.precision(2);
  oss << _("Magnitude: ") << getMagnitude(nav)
      << " B-V: " << s->getBV()
      << endl;
  oss << _("J2000") << L" " << _("RA/DE: ")
      << StelUtils::printAngleHMS(ra_j2000,true)
      << L"/" << StelUtils::printAngleDMS(dec_j2000,true) << endl;
///  oss << "Motion J2000: " << s->dx0 << '/' << s->dx1 << endl;


  oss << _("Equ of date") << L" " << _("RA/DE: ")
      << StelUtils::printAngleHMS(ra_equ)
      << L"/" << StelUtils::printAngleDMS(dec_equ) << endl;

    // calculate alt az
  double az,alt;
  StelUtils::rect_to_sphe(&az,&alt,nav->earth_equ_to_local(equatorial_pos));
  az = 3*M_PI - az;  // N is zero, E is 90 degrees
  if(az > M_PI*2) az -= M_PI*2;    
  oss << _("Az/Alt: ") << StelUtils::printAngleDMS(az)
      << L"/" << StelUtils::printAngleDMS(alt) << endl;

  if (s->plx) {
    oss.precision(5);
    oss << _("Parallax: ") << (0.00001*s->plx) << endl;
    oss.precision(2);
    oss << _("Distance: ")
        << (AU/(SPEED_OF_LIGHT*86400*365.25))
                       / (s->plx*((0.00001/3600)*(M_PI/180)))
        << _(" Light Years") << endl;
  }

  if (s->sp_int) {
    oss << _("Spectral Type: ")
        << convertToSpectralType(s->sp_int).c_str() << endl;
  }
  oss.precision(2);

  return oss.str();
}


wstring StarWrapper1::getShortInfoString(const Navigator *nav) const {
	wostringstream oss;
	if (s->hip) {
		const wstring commonNameI18 = StarMgr::getCommonName(s->hip);
		const wstring sciName = StarMgr::getSciName(s->hip);
		if (commonNameI18!=L"" || sciName!=L"") {
			oss << commonNameI18 << (commonNameI18 == L"" ? L"" : L" ");
			if (commonNameI18!=L"" && sciName!=L"") oss << L"(";
			oss << wstring(sciName==L"" ? L"" : sciName);
			if (commonNameI18!=L"" && sciName!=L"") oss << L")";
			oss << L"  ";
		}
		oss << L"HP " << s->hip;
		if (s->component_ids) {
			oss << L" "
				<< convertToComponentIds(s->component_ids).c_str();
		}
		oss << L"  ";
	}
	
	oss.setf(ios::fixed);
	oss.precision(2);
	oss << _("Magnitude: ") << getMagnitude(nav);
	oss << L"  ";

	if (s->plx) {
		oss.precision(2);
		oss << _("Distance: ")
			<< (AU/(SPEED_OF_LIGHT*86400*365.25))
			/ (s->plx*((0.00001/3600)*(M_PI/180)))
			<< _(" Light Years") << L"  ";
	}
	
	if (s->sp_int) {
		oss << _("Spectral Type: ")
			<< convertToSpectralType(s->sp_int).c_str();
	}
	oss.precision(2);
	
	return oss.str();
}






class StarWrapper2 : public StarWrapper<Star2> {
public:
  StarWrapper2(const SpecialZoneArray<Star2> *a,
               const SpecialZoneData<Star2> *z,
               const Star2 *s) : StarWrapper<Star2>(a,z,s) {}
};

class StarWrapper3 : public StarWrapper<Star3> {
public:
  StarWrapper3(const SpecialZoneArray<Star3> *a,
               const SpecialZoneData<Star3> *z,
               const Star3 *s) : StarWrapper<Star3>(a,z,s) {}
};


StelObjectP Star1::createStelObject(const SpecialZoneArray<Star1> *a,
                                    const SpecialZoneData<Star1> *z) const {
  return StelObjectP(new StarWrapper1(a,z,this));
}

StelObjectP Star2::createStelObject(const SpecialZoneArray<Star2> *a,
                                    const SpecialZoneData<Star2> *z) const {
  return StelObjectP(new StarWrapper2(a,z,this));
}

StelObjectP Star3::createStelObject(const SpecialZoneArray<Star3> *a,
                                    const SpecialZoneData<Star3> *z) const {
  return StelObjectP(new StarWrapper3(a,z,this));
}


void ZoneArray1::updateHipIndex(HipIndexStruct hip_index[]) const {
  for (const SpecialZoneData<Star1> *z=getZones()+nr_of_zones-1;
       z>=getZones();z--) {
    for (const Star1 *s = z->getStars()+z->size-1;s>=z->getStars();s--) {
      const int hip = s->hip;
      assert(0 <= hip && hip <= NR_OF_HIP);
      if (hip != 0) {
        hip_index[hip].a = this;
        hip_index[hip].z = z;
        hip_index[hip].s = s;
      }
    }
  }
}


static inline
int ReadInt(FILE *f,int &x) {
  const int rval = (4 == fread(&x,1,4,f)) ? 0 : -1;
  LE_TO_CPU_INT32(x,x);
  return rval;
}


#define FILE_MAGIC 0x835f040a
#define MAX_MAJOR_FILE_VERSION 0

ZoneArray *ZoneArray::create(const StarMgr &hip_star_mgr,
                             const char *fname,
                             LoadingBar &lb) {
  ZoneArray *rval = 0;
  FILE *f = fopen(fname,"rb");
  if (f == 0) {
    fprintf(stderr,"ZoneArray::create(%s): fopen failed\n",fname);
  } else {
    printf("ZoneArray::create(%s): ",fname);
    int magic,major,minor,type,level,mag_min,mag_range,mag_steps;
    if (ReadInt(f,magic) < 0 ||
        ReadInt(f,type) < 0 ||
        ReadInt(f,major) < 0 ||
        ReadInt(f,minor) < 0 ||
        ReadInt(f,level) < 0 ||
        ReadInt(f,mag_min) < 0 ||
        ReadInt(f,mag_range) < 0 ||
        ReadInt(f,mag_steps) < 0) {
      printf("bad file, ");
    } else if (((unsigned int)magic) != FILE_MAGIC) {
      printf("no star catalogue file, ");
    } else {
      printf("type: %d major: %d minor: %d level: %d"
             " mag_min: %d mag_range: %d mag_steps: %d; ",
             type,major,minor,level,mag_min,mag_range,mag_steps);
      switch (type) {
        case 0:
          if (major < 0 || major > MAX_MAJOR_FILE_VERSION) {
            printf("unsupported version, ");
          } else {
              // When this assertion fails you must redefine Star1
              // for your compiler.
              // Because your compiler does not pack the data,
              // which is crucial for this application.
            assert(sizeof(Star1) == 28);
            rval = new ZoneArray1(f,lb,hip_star_mgr,level,
                                  mag_min,mag_range,mag_steps);
            if (rval == 0) {
              printf("no memory, ");
            }
          }
          break;
        case 1:
          if (major < 0 || major > MAX_MAJOR_FILE_VERSION) {
            printf("unsupported version, ");
          } else {
              // When this assertion fails you must redefine Star2
              // for your compiler.
              // Because your compiler does not pack the data,
              // which is crucial for this application.
            assert(sizeof(Star2) == 10);
            rval = new SpecialZoneArray<Star2>(f,lb,hip_star_mgr,level,
                                               mag_min,mag_range,mag_steps);
            if (rval == 0) {
              printf("no memory, ");
            }
          }
          break;
        case 2:
          if (major < 0 || major > MAX_MAJOR_FILE_VERSION) {
            printf("unsupported version, ");
          } else {
              // When this assertion fails you must redefine Star3
              // for your compiler.
              // Because your compiler does not pack the data,
              // which is crucial for this application.
            assert(sizeof(Star3) == 6);
            rval = new SpecialZoneArray<Star3>(f,lb,hip_star_mgr,level,
                                               mag_min,mag_range,mag_steps);
            if (rval == 0) {
              printf("no memory, ");
            }
          }
          break;
        default:
          printf("bad file type, ");
          break;
      }
      if (rval && rval->isInitialized()) {
        printf("stars: %d\n",rval->getNrOfStars());
      } else {
        printf("initialization failed\n");
        if (rval) {
          delete rval;
          rval = 0;
        }
      }
    }
    fclose(f);
  }
  return rval;
}



ZoneArray::ZoneArray(const StarMgr &hip_star_mgr,int level,
                     int mag_min,int mag_range,int mag_steps)
          :level(level),
           mag_min(mag_min),mag_range(mag_range),mag_steps(mag_steps),
           star_position_scale(0.0), //IntToDouble(scale_int)/Star::max_pos_val
           hip_star_mgr(hip_star_mgr),
           zones(0) {
  nr_of_zones = GeodesicGrid::nrOfZones(level);
  nr_of_stars = 0;
}

struct TmpZoneData {
  int size;
};


template<class Star>
SpecialZoneArray<Star>::SpecialZoneArray(FILE *f,LoadingBar &lb,
                                         const StarMgr &hip_star_mgr,
                                         int level,
                                         int mag_min,int mag_range,
                                         int mag_steps)
                       :ZoneArray(hip_star_mgr,level,
                                  mag_min,mag_range,mag_steps),
                        stars(0) {
  if (nr_of_zones > 0) {
//		lb.Draw(i / 8.0);
    zones = new SpecialZoneData<Star>[nr_of_zones];
    assert(zones!=0);
    {
      int *zone_size = new int[nr_of_zones];
      assert(zone_size!=0);
      if (nr_of_zones != (int)fread(zone_size,sizeof(int),
                                    nr_of_zones,f)) {
        delete[] getZones();
        zones = 0;
        nr_of_zones = 0;
      } else {
        const int *tmp = zone_size;
        for (int z=0;z<nr_of_zones;z++,tmp++) {
          int tmp_spu_int32;
          LE_TO_CPU_INT32(tmp_spu_int32,*tmp);
          nr_of_stars += tmp_spu_int32;
          getZones()[z].size = tmp_spu_int32;
        }
      }
        // delete zone_size before allocating stars
        // in order to avoid memory fragmentation:
      delete[] zone_size;
    }
    
    if (nr_of_stars <= 0) {
        // no stars ?
      if (zones) delete[] getZones();
      zones = 0;
      nr_of_zones = 0;
    } else {
      stars = new Star[nr_of_stars];
      assert(stars!=0);
      if (nr_of_stars != (int)fread(stars,sizeof(Star),nr_of_stars,f)) {
        delete[] stars;
        stars = 0;
        nr_of_stars = 0;
        delete[] getZones();
        zones = 0;
        nr_of_zones = 0;
      } else {
        Star *s = stars;
        for (int z=0;z<nr_of_zones;z++) {
          getZones()[z].stars = s;
          s += getZones()[z].size;
        }
#if (defined(WORDS_BIGENDIAN) || !defined(__GNUC__))
        s = stars;
        for (int i=0;i<nr_of_stars;i++,s++) s->repack();
#endif
//        cout << endl
//             << "SpecialZoneArray<Star>::SpecialZoneArray(" << level
//             << "): repack test start" << endl;
//        stars[0].print();
//        stars[1].print();
//        cout << "SpecialZoneArray<Star>::SpecialZoneArray(" << level
//             << "): repack test end" << endl;
      }
    }
  }
}



StarMgr::StarMgr(void) :
    limitingMag(6.5f),
    starTexture(),
    hip_index(new HipIndexStruct[NR_OF_HIP+1]),
	fontSize(13.),
    starFont(0)
{
  assert(hip_index);
  max_geodesic_grid_level = -1;
  last_max_search_level = -1;
  dependenciesOrder["draw"]="constellations";
}


StarMgr::~StarMgr(void) {
  ZoneArrayMap::iterator it(zone_arrays.end());
  while (it!=zone_arrays.begin()) {
    --it;
    delete it->second;
    it->second = NULL;
  }
  zone_arrays.clear();

  if (hip_index) delete[] hip_index;
}

bool StarMgr::flagSciNames = true;
map<int,string> StarMgr::common_names_map;
map<int,wstring> StarMgr::common_names_map_i18n;
map<string,int> StarMgr::common_names_index;
map<wstring,int> StarMgr::common_names_index_i18n;

map<int,wstring> StarMgr::sci_names_map_i18n;
map<wstring,int> StarMgr::sci_names_index_i18n;

wstring StarMgr::getCommonName(int hip) {
  map<int,wstring>::const_iterator it(common_names_map_i18n.find(hip));
  if (it!=common_names_map_i18n.end()) return it->second;
  return L"";
}

wstring StarMgr::getSciName(int hip) {
  map<int,wstring>::const_iterator it(sci_names_map_i18n.find(hip));
  if (it!=sci_names_map_i18n.end()) return it->second;
  return L"";
}

void StarMgr::init(const InitParser &conf,LoadingBar &lb) {
  load_data(conf,lb);
  StelApp::getInstance().getTextureManager().setDefaultParams();
    // Load star texture no mipmap:
  starTexture = StelApp::getInstance().getTextureManager().createTexture("star16x16.png");
  double fontSize = 12;
  starFont = &StelApp::getInstance().getFontManager()
                .getStandardFont(StelApp::getInstance()
                                   .getLocaleMgr().getSkyLanguage(),
                                 fontSize);
  setFlagStars(conf.get_boolean("astro:flag_stars"));
  setFlagNames(conf.get_boolean("astro:flag_star_name"));
  setScale(conf.get_double ("stars:star_scale"));
  setMagScale(conf.get_double ("stars:star_mag_scale"));
  setTwinkleAmount(conf.get_double ("stars:star_twinkle_amount"));
  setMaxMagName(conf.get_double ("stars:max_mag_star_name"));
  setFlagTwinkle(conf.get_boolean("stars:flag_star_twinkle"));
  setFlagPointStar(conf.get_boolean("stars:flag_point_star"));
  setLimitingMag(conf.get_double("stars", "star_limiting_mag", 6.5f));
  
  StelApp::getInstance().getStelObjectMgr().registerStelObjectMgr(this);
  
  texPointer = StelApp::getInstance().getTextureManager().createTexture("pointeur2.png");   // Load pointer texture
}

void StarMgr::setGrid(void) {
  geodesic_grid->visitTriangles(max_geodesic_grid_level,initTriangleFunc,this);
  for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
       it!=zone_arrays.end();it++) {
    it->second->scaleAxis();
  }
}


void StarMgr::drawPointer(const Projector* prj, const Navigator * nav)
{
	const std::vector<StelObjectP> newSelected = StelApp::getInstance().getStelObjectMgr().getSelectedObject("Star");
	if (!newSelected.empty())
	{
		const StelObjectP obj = newSelected[0];
		Vec3d pos=obj->getObsJ2000Pos(nav);
		Vec3d screenpos;
		// Compute 2D pos and return if outside screen
		if (!prj->project(pos, screenpos)) return;
	
		glColor3fv(obj->getInfoColor());
		float diameter = 26.f;
		texPointer->bind();
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Normal transparency mode
        prj->drawSprite2dMode(screenpos[0], screenpos[1], diameter, StelApp::getInstance().getTotalRunTime()*40.);
	}
}

void StarMgr::setColorScheme(const InitParser& conf, const std::string& section)
{
	// Load colors from config file
	string defaultColor = conf.get_str(section,"default_color");
	setLabelColor(StelUtils::str_to_vec3f(conf.get_str(section,"star_label_color", defaultColor)));
	setCircleColor(StelUtils::str_to_vec3f(conf.get_str(section,"star_circle_color", defaultColor)));
}

/***************************************************************************
 Load star catalogue data from files.
 If a file is not found, it will be skipped.
***************************************************************************/
void StarMgr::load_data(const InitParser &baseConf,LoadingBar &lb)
{
	// Please do not init twice:
	assert(max_geodesic_grid_level < 0);

	cout << "Loading star data...";

	const string starsIniFile = StelApp::getInstance().getFileMgr().findFile(string("stars/default/stars.ini"));
	InitParser conf;
	conf.load(starsIniFile);
				         
	for (int i=0; i<100; i++)
	{
		char key_name[64];
		sprintf(key_name,"cat_file_name_%02d",i);
		const string cat_file_name = conf.get_str("stars",key_name,"");
		if (!cat_file_name.empty()) {
			lb.SetMessage(_("Loading catalog ")
			              + StelUtils::stringToWstring(cat_file_name));
			try
			{
				ZoneArray *const z
				   = ZoneArray::create(*this,
				       StelApp::getInstance().getFileMgr()
				         .findFile(string("stars/default/")+cat_file_name)
				                    .c_str(),lb);
				if (z)
				{
					if (max_geodesic_grid_level < z->level)
					{
						max_geodesic_grid_level = z->level;
					}
					ZoneArray *&pos(zone_arrays[z->level]);
					if (pos)
					{
						cerr << cat_file_name << ", " << z->level
						     << ": duplicate level" << endl;
						delete z;
					}
					else
					{
						pos = z;
					}
				}
			}
			catch (exception& e)
			{
				cerr << "error while loading " << cat_file_name
				     << ": " << e.what() << endl;
			}
		}
	}

	for (int i=0; i<=NR_OF_HIP; i++)
	{
		hip_index[i].a = 0;
		hip_index[i].z = 0;
		hip_index[i].s = 0;
	}
	for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
	                it != zone_arrays.end();it++)
	{
		it->second->updateHipIndex(hip_index);
	}

	const string cat_hip_sp_file_name
	  = conf.get_str("stars","cat_hip_sp_file_name","");
	if (cat_hip_sp_file_name.empty())
	{
		cerr << "ERROR: stars:cat_hip_sp_file_name not found" << endl;
	}
	else
	{
		try
		{
			spectral_array.initFromFile(StelApp::getInstance().getFileMgr()
			        .findFile("stars/default/" + cat_hip_sp_file_name).c_str());
		}
		catch (exception& e)
		{
			cerr << "ERROR while loading data from "
			     << ("stars/default/" + cat_hip_sp_file_name)
		    	 << ": " << e.what() << endl;
		}
	}

	const string cat_hip_cids_file_name
	  = conf.get_str("stars",cat_hip_cids_file_name,"");
	if (cat_hip_cids_file_name.empty())
	{
		cerr << "ERROR: stars:cat_hip_cids_file_name not found" << endl;
	}
	else
	{
		try
		{
			component_array.initFromFile(StelApp::getInstance().getFileMgr()
			        .findFile("stars/default/" + cat_hip_cids_file_name)
			        .c_str());
		}
		catch (exception& e)
		{
			cerr << "ERROR while loading data from "
			     << ("stars/default/" + cat_hip_cids_file_name)
		    	 << ": " << e.what() << endl;
		}
	}

	last_max_search_level = max_geodesic_grid_level;
	cout << "finished, max_geodesic_level: " << max_geodesic_grid_level << endl;
}

// Load common names from file 
int StarMgr::load_common_names(const string& commonNameFile) {
  cout << "Load star names from " << commonNameFile << endl;

  FILE *cnFile;
  cnFile=fopen(commonNameFile.c_str(),"r");
  if (!cnFile) {
    cerr << "Warning " << commonNameFile << " not found." << endl;
    return 0;
  }

  common_names_map.clear();
  common_names_map_i18n.clear();
  common_names_index.clear();
  common_names_index_i18n.clear();

  // Assign names to the matching stars, now support spaces in names
  char line[256];
  while (fgets(line, sizeof(line), cnFile)) {
    line[sizeof(line)-1] = '\0';
    unsigned int hip;
    if(sscanf(line,"%u",&hip)!=1)
		assert(0);
    unsigned int i = 0;
    while (line[i]!='|' && i<sizeof(line)-2) ++i;
    i++;
    string englishCommonName =  line+i;
    // remove newline
    englishCommonName.erase(englishCommonName.length()-1, 1);

//cout << hip << ": \"" << englishCommonName << '"' << endl;

    // remove underscores
    for (string::size_type j=0;j<englishCommonName.length();++j) {
        if (englishCommonName[j]=='_') englishCommonName[j]=' ';
    }
    const wstring commonNameI18n = _(englishCommonName.c_str());
    wstring commonNameI18n_cap = commonNameI18n;
    transform(commonNameI18n.begin(), commonNameI18n.end(),
              commonNameI18n_cap.begin(), ::toupper);

    common_names_map[hip] = englishCommonName;
    common_names_index[englishCommonName] = hip;
    common_names_map_i18n[hip] = commonNameI18n;
    common_names_index_i18n[commonNameI18n_cap] = hip;
  }

  fclose(cnFile);
  return 1;
}


// Load scientific names from file 
void StarMgr::load_sci_names(const string& sciNameFile) {
  cout << "Load sci names from " << sciNameFile << endl;

  FILE *snFile;
  snFile=fopen(sciNameFile.c_str(),"r");
  if (!snFile) {
    cerr << "Warning " << sciNameFile.c_str() << " not found" << endl;
    return;
  }

  sci_names_map_i18n.clear();
  sci_names_index_i18n.clear();

  // Assign names to the matching stars, now support spaces in names
  char line[256];
  while (fgets(line, sizeof(line), snFile)) {
    line[sizeof(line)-1] = '\0';
    unsigned int hip;
    if(sscanf(line,"%u",&hip)!=1)
		assert(0);
    unsigned int i = 0;
    while (line[i]!='|' && i<sizeof(line)-2) ++i;
    i++;
    char *tempc = line+i;
    while (line[i]!='_' && i<sizeof(line)-1) ++i;
    line[i-1]=' ';
    string sci_name = tempc;
    sci_name.erase(sci_name.length()-1, 1);
    wstring sci_name_i18n = StelUtils::stringToWstring(sci_name);

    // remove underscores
    for (wstring::size_type j=0;j<sci_name_i18n.length();++j) {
      if (sci_name_i18n[j]==L'_') sci_name_i18n[j]=L' ';
    }

    wstring sci_name_i18n_cap = sci_name_i18n;
    transform(sci_name_i18n.begin(), sci_name_i18n.end(),
              sci_name_i18n_cap.begin(), ::toupper);
    
    sci_names_map_i18n[hip] = sci_name_i18n;
    sci_names_index_i18n[sci_name_i18n_cap] = hip;
  }

  fclose(snFile);
}












int StarMgr::drawStar(const Projector *prj, const Vec3d &XY,float rmag,const Vec3f &color) const {
//cout << "StarMgr::drawStar: " << XY[0] << '/' << XY[1] << ", " << rmag << endl;
  float cmag = 1.f;

  // if size of star is too small (blink) we put its size to 1.2 --> no more blink
  // And we compensate the difference of brighteness with cmag
  if (rmag<1.2f) {
    cmag = rmag*rmag/1.44f;
    if (rmag < 0.1f*star_scale ||
        cmag * starMagScale < 0.1) return -1;
    rmag=1.2f;
  } else {
//if (rmag>4.f) {
//  rmag=4.f+2.f*std::sqrt(1.f+rmag-4.f)-2.f;
  if (rmag>8.f) {
    rmag=8.f+2.f*std::sqrt(1.f+rmag-8.f)-2.f;
  }
//}
//    if (rmag > 5.f) {
//      rmag=5.f;
//    }
  }

  // Calculation of the luminosity
  // Random coef for star twinkling
  cmag *= (1.-twinkle_amount*rand()/RAND_MAX);

  // Global scaling
  rmag *= star_scale;
  cmag *= starMagScale;

  glColor3fv(color*cmag);

    // Blending is really important. Otherwise faint stars in the vicinity of
    // bright star will cause tiny black squares on the bright star, e.g.
    // see Procyon.
  glBlendFunc(GL_ONE, GL_ONE);

	prj->drawSprite2dMode(XY[0], XY[1], 2*rmag);
  return 0;
}


int StarMgr::drawPointStar(const Projector *prj, const Vec3d &XY,float rmag,
                              const Vec3f &color) const {
  if (rmag < 0.05f*star_scale) return -1;
  float cmag = rmag * rmag / 1.44f;
  if (cmag*starMagScale < 0.05) return -2;

  cmag *= (1.-twinkle_amount*rand()/RAND_MAX);
  cmag *= starMagScale;

  glColor3fv(color*cmag);

  glBlendFunc(GL_ONE, GL_ONE);

  prj->drawPoint2d(XY[0], XY[1]);

  return 0;
}


template<class Star>
void SpecialZoneArray<Star>::draw(int index,bool is_inside,
                                  bool draw_point,
                                  const float *rmag_table,
                                  Projector *prj,
                                  unsigned int max_mag_star_name,
                                  float names_brightness,
                                  SFont *starFont,
                                  STextureSP starTexture) const {
  if (draw_point) {
    glDisable(GL_TEXTURE_2D);
    glPointSize(0.1);
  }
  SpecialZoneData<Star> *const z = getZones() + index;
  Vec3d xy;
  const Star *const end = z->getStars() + z->size;
  const double movement_factor = (M_PI/180)*(0.0001/3600)
                           * ((current_JDay-d2000)/365.25)
                           / star_position_scale;            
  for (const Star *s=z->getStars();s<end;s++) {
    if (is_inside
        ? prj->project(s->getJ2000Pos(z,movement_factor),xy)
        : prj->projectCheck(s->getJ2000Pos(z,movement_factor),xy)) {
      if (0 > (draw_point ? hip_star_mgr.drawPointStar(prj, xy,rmag_table[s->mag],
                                                       color_table[s->b_v])
                          : hip_star_mgr.drawStar(prj, xy,rmag_table[s->mag],
                                                  color_table[s->b_v]))) {
        break;
      }
      if (s->mag < max_mag_star_name) {
        const wstring starname = s->getNameI18n();
        if (!starname.empty()) {
          glColor4f(color_table[s->b_v][0]*0.75,
                    color_table[s->b_v][1]*0.75,
                    color_table[s->b_v][2]*0.75,
                    names_brightness);
          if (draw_point) {
            glEnable(GL_TEXTURE_2D);
          }

          prj->drawText(starFont,xy[0],xy[1], starname, 0, 4, 4, false);
          if (draw_point) {
            glDisable(GL_TEXTURE_2D);
          } else {
            starTexture->bind();
          }
        }
      }
    }
  }
  if (draw_point) {
    glEnable(GL_TEXTURE_2D);
  }
}





int StarMgr::getMaxSearchLevel(const ToneReproducer *eye,
                                  const Projector *prj) const {
  int rval = -1;
  float fov_q = prj->getFov();
  if (fov_q > 60) fov_q = 60;
  else if (fov_q < 0.1) fov_q = 0.1;
  fov_q = 1.f/(fov_q*fov_q);
  for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
       it!=zone_arrays.end();it++) {
    const float mag_min = 0.001f*it->second->mag_min;
    const float rmag =
      std::sqrt(eye->adapt_luminance(
        std::exp(-0.92103f*(mag_min + 12.12331f)) * 108064.73f * fov_q)) * 30.f;
    if (rmag<1.2f) {
      const float cmag = rmag*rmag/1.44f;
      if (rmag < 0.1f*starScale ||
          cmag * starMagScale < 0.1) break;
    }
    rval = it->first;
  }
  return rval;
}


// Draw all the stars
double StarMgr::draw(Projector *prj, const Navigator *nav, ToneReproducer *eye) {
    current_JDay = nav->getJDay();

    // If stars are turned off don't waste time below
    // projecting all stars just to draw disembodied labels
    if(!starsFader.getInterstate()) return 0.;

    float fov_q = prj->getFov();
    if (fov_q > 60) fov_q = 60;
    else if (fov_q < 0.1) fov_q = 0.1;
    fov_q = 1.f/(fov_q*fov_q);

    // Set temporary static variable for optimization
    if (flagStarTwinkle) twinkle_amount = twinkleAmount;
    else twinkle_amount = 0;
    star_scale = starScale * starsFader.getInterstate();
    const float names_brightness = names_fader.getInterstate() 
                                 * starsFader.getInterstate();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    
    // FOV is currently measured vertically, so need to adjust for wide screens
    // TODO: projector should probably use largest measurement itself
//    const float max_fov =
//      MY_MAX( prj->getFov(),
//              prj->getFov()*prj->getViewportWidth()/prj->getViewportHeight());
    //    float maxMag = limiting_mag-1 + 60.f/prj->getFov();
//    const float maxMag = limitingMag-1 + 60.f/max_fov;

    prj->setCurrentFrame(Projector::FRAME_J2000);

    // Bind the star texture
    starTexture->bind();

    // Set the draw mode
    glBlendFunc(GL_ONE, GL_ONE);

    // draw all the stars of all the selected zones
    float rmag_table[256];
//static int count = 0;
//count++;
    for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
         it!=zone_arrays.end();it++) {
      const float mag_min = 0.001f*it->second->mag_min;
//      if (maxMag < mag_min) break;
      const float k = (0.001f*it->second->mag_range)/it->second->mag_steps;
      for (int i=it->second->mag_steps-1;i>=0;i--) {
        const float mag = mag_min+k*i;
          // Compute the equivalent star luminance for a 5 arc min circle
          // and convert it in function of the eye adaptation
//        rmag_table[i] =
//          eye->adapt_luminance(std::exp(-0.92103f*(mag + 12.12331f)) * 108064.73f)
//            * std::pow(prj->getFov(),-0.85f) * 70.f;
        rmag_table[i] =
          std::sqrt(eye->adapt_luminance(
            std::exp(-0.92103f*(mag + 12.12331f)) * 108064.73f * fov_q)) * 30.f;
        if (i==0 && rmag_table[0]<1.2f) {
          const float cmag = rmag_table[0]*rmag_table[0]/1.44f;
          if (rmag_table[0] < 0.1f*star_scale ||
              cmag * starMagScale < 0.1) goto exit_loop;
        }
      }
      last_max_search_level = it->first;

      unsigned int max_mag_star_name = 0;
      if (names_fader.getInterstate()) {
        int x = (int)((maxMagStarName-mag_min)/k);
        if (x > 0) max_mag_star_name = x;
      }
      int zone;
//if ((count&63)==0) cout << "inside(" << it->first << "):";
      for (GeodesicSearchInsideIterator it1(*geodesic_search_result,it->first);
           (zone = it1.next()) >= 0;) {
        it->second->draw(zone,true,flagPointStar,rmag_table,prj,
                         max_mag_star_name,names_brightness,
                         starFont,starTexture);
//if ((count&63)==0) cout << " " << zone;
      }
//if ((count&63)==0) cout << endl << "border(" << it->first << "):";
      for (GeodesicSearchBorderIterator it1(*geodesic_search_result,it->first);
           (zone = it1.next()) >= 0;) {
        it->second->draw(zone,false,flagPointStar,rmag_table,prj,
                         max_mag_star_name,names_brightness,
                         starFont,starTexture);
//if ((count&63)==0) cout << " " << zone;
      }
//if ((count&63)==0) cout << endl;
    }
    exit_loop:
//if ((count&63)==0) cout << endl;

	drawPointer(prj, nav);

    return 0.;
}






// Look for a star by XYZ coords
StelObjectP StarMgr::search(Vec3d pos) const {
assert(0);
  pos.normalize();
  vector<StelObjectP > v = searchAround(pos,
                                      0.8, // just an arbitrary number
                                      NULL, NULL);
  StelObjectP nearest;
  double cos_angle_nearest = -10.0;
  for (vector<StelObjectP >::const_iterator it(v.begin());it!=v.end();it++) {
    const double c = (*it)->getObsJ2000Pos(0)*pos;
    if (c > cos_angle_nearest) {
      cos_angle_nearest = c;
      nearest = *it;
    }
  }
  return nearest;
}

// Return a stl vector containing the stars located
// inside the lim_fov circle around position v
vector<StelObjectP > StarMgr::searchAround(const Vec3d& vv,
                                            double lim_fov, // degrees
                                            const Navigator * nav,
                                            const Projector * prj) const {
  vector<StelObjectP > result;
  if (!getFlagStars())
  	return result;
  	
  Vec3d v(vv);
  v.normalize();
    // find any vectors h0 and h1 (length 1), so that h0*v=h1*v=h0*h1=0
  int i;
  {
    const double a0 = fabs(v[0]);
    const double a1 = fabs(v[1]);
    const double a2 = fabs(v[2]);
    if (a0 <= a1) {
      if (a0 <= a2) i = 0;
      else i = 2;
    } else {
      if (a1 <= a2) i = 1;
      else i = 2;
    }
  }
  Vec3d h0(0.0,0.0,0.0);
  h0[i] = 1.0;
  Vec3d h1 = h0 ^ v;
  h1.normalize();
  h0 = h1 ^ v;
  h0.normalize();
    // now we have h0*v=h1*v=h0*h1=0.
    // construct a region with 4 corners e0,e1,e2,e3 inside which
    // all desired stars must be:
  double f = 1.4142136 * tan(lim_fov * M_PI/180.0);
  h0 *= f;
  h1 *= f;
  Vec3d e0 = v + h0;
  Vec3d e1 = v + h1;
  Vec3d e2 = v - h0;
  Vec3d e3 = v - h1;
  f = 1.0/e0.length();
  e0 *= f;
  e1 *= f;
  e2 *= f;
  e3 *= f;
    // search the triangles
  geodesic_search_result->search(e0,e1,e2,e3,last_max_search_level);
    // iterate over the stars inside the triangles:
  f = cos(lim_fov * M_PI/180.);
  for (ZoneArrayMap::const_iterator it(zone_arrays.begin());
       it!=zone_arrays.end();it++) {
//cout << "search inside(" << it->first << "):";
    int zone;
    for (GeodesicSearchInsideIterator it1(*geodesic_search_result,it->first);
         (zone = it1.next()) >= 0;) {
      it->second->searchAround(zone,v,f,result);
//cout << " " << zone;
    }
//cout << endl << "search border(" << it->first << "):";
    for (GeodesicSearchBorderIterator it1(*geodesic_search_result,it->first);
         (zone = it1.next()) >= 0;) {
      it->second->searchAround(zone,v,f,result);
//cout << " " << zone;
    }
//cout << endl << endl;
  }
  return result;
}






template<class Star>
void SpecialZoneArray<Star>::searchAround(int index,const Vec3d &v,
                                          double cos_lim_fov,
                                          vector<StelObjectP > &result) {
  const double movement_factor = (M_PI/180)*(0.0001/3600)
                           * ((current_JDay-d2000)/365.25)
                           / star_position_scale;
  const SpecialZoneData<Star> *const z = getZones()+index;
  for (int i=0;i<z->size;i++) {
    if (z->getStars()[i].getJ2000Pos(z,movement_factor)*v >= cos_lim_fov) {
      result.push_back(z->getStars()[i].createStelObject(this,z));
    }
  }
}

//! @brief Update i18 names from english names according to passed translator
//! The translation is done using gettext with translated strings defined in translations.h
void StarMgr::updateI18n() {
  Translator trans = StelApp::getInstance().getLocaleMgr().getSkyTranslator();
  common_names_map_i18n.clear();
  common_names_index_i18n.clear();
  for (map<int,string>::iterator it(common_names_map.begin());
       it!=common_names_map.end();it++) {
    const int i = it->first;
    const wstring t(trans.translate(it->second));
    common_names_map_i18n[i] = t;
    wstring t_cap = t;
    transform(t.begin(), t.end(), t_cap.begin(), ::toupper);
    common_names_index_i18n[t_cap] = i;
  }
  starFont = &StelApp::getInstance().getFontManager().getStandardFont(trans.getTrueLocaleName(), fontSize);
}


StelObjectP StarMgr::search(const string& name) const
{
    const string catalogs("HP HD SAO");

    string n = name;
    for (string::size_type i=0;i<n.length();++i)
    {
        if (n[i]=='_') n[i]=' ';
    }    
    
    istringstream ss(n);
    string cat;
    unsigned int num;
    
    ss >> cat;
    
    // check if a valid catalog reference
    if (catalogs.find(cat,0) == string::npos)
    {
        // try see if the string is a HP number
        istringstream cat_to_num(cat);
        cat_to_num >> num;
        if (!cat_to_num.fail()) return searchHP(num);
        return NULL;
    }

    ss >> num;
    if (ss.fail()) return NULL;

    if (cat == "HP") return searchHP(num);
    assert(0);
    return NULL;
}    

// Search the star by HP number
StelObjectP StarMgr::searchHP(int _HP) const {
  if (0 < _HP && _HP <= NR_OF_HIP) {
    const Star1 *const s = hip_index[_HP].s;
    if (s) {
      const SpecialZoneArray<Star1> *const a = hip_index[_HP].a;
      const SpecialZoneData<Star1> *const z = hip_index[_HP].z;
      return s->createStelObject(a,z);
    }
  }
  return StelObjectP();
}

StelObjectP StarMgr::searchByNameI18n(const wstring& nameI18n) const
{
    wstring objw = nameI18n;
    transform(objw.begin(), objw.end(), objw.begin(), ::toupper);
    
    // Search by HP number if it's an HP formated number
    // Please help, if you know a better way to do this:
    if (nameI18n.length() >= 2 && nameI18n[0]==L'H' && nameI18n[1]==L'P')
    {
        bool hp_ok = false;
        wstring::size_type i=2;
        // ignore spaces
        for (;i<nameI18n.length();i++)
        {
            if (nameI18n[i] != L' ') break;
        }
        // parse the number
        unsigned int nr = 0;
        for (;i<nameI18n.length();i++)
        {
            if (hp_ok = (L'0' <= nameI18n[i] && nameI18n[i] <= L'9'))
            {
                nr = 10*nr+(nameI18n[i]-L'0');
            }
            else
            {
                break;
            }
        }
        if (hp_ok)
        {
            return searchHP(nr);
        }
    }

    // Search by I18n common name
  map<wstring,int>::const_iterator it(common_names_index_i18n.find(objw));
  if (it!=common_names_index_i18n.end()) {
    return searchHP(it->second);
  }

    // Search by sci name
  it = sci_names_index_i18n.find(objw);
  if (it!=sci_names_index_i18n.end()) {
    return searchHP(it->second);
  }

  return StelObjectP();
}


StelObjectP StarMgr::searchByName(const string& name) const
{
    wstring objw = StelUtils::stringToWstring(name);
    transform(objw.begin(), objw.end(), objw.begin(), ::toupper);
    
    // Search by HP number if it's an HP formated number
    // Please help, if you know a better way to do this:
    if (name.length() >= 2 && name[0]=='H' && name[1]=='P')
    {
        bool hp_ok = false;
        string::size_type i=2;
        // ignore spaces
        for (;i<name.length();i++)
        {
            if (name[i] != ' ') break;
        }
        // parse the number
        unsigned int nr = 0;
        for (;i<name.length();i++)
        {
            if (hp_ok = ('0' <= name[i] && name[i] <= '9'))
            {
                nr = 10*nr+(name[i]-'0');
            }
            else
            {
                break;
            }
        }
        if (hp_ok)
        {
            return searchHP(nr);
        }
    }


  map<wstring,int>::const_iterator it(common_names_index_i18n.find(objw));

/* Should we try this anyway?
    // Search by common name

  if (it!=common_names_index_i18n.end()) {
    return searchHP(it->second);
  }
*/
    // Search by sci name
  it = sci_names_index_i18n.find(objw);
  if (it!=sci_names_index_i18n.end()) {
    return searchHP(it->second);
  }

  return StelObjectP();
}

//! Find and return the list of at most maxNbItem objects auto-completing
//! the passed object I18n name
vector<wstring> StarMgr::listMatchingObjectsI18n(
                              const wstring& objPrefix,
                              unsigned int maxNbItem) const {
  vector<wstring> result;
  if (maxNbItem==0) return result;

  wstring objw = objPrefix;
  transform(objw.begin(), objw.end(), objw.begin(), ::toupper);

    // Search for common names
  for (map<wstring,int>::const_iterator
       it(common_names_index_i18n.lower_bound(objw));
       it!=common_names_index_i18n.end();it++) {
    const wstring constw(it->first.substr(0,objw.size()));
    if (constw == objw) {
      if (maxNbItem == 0) break;
      result.push_back(getCommonName(it->second));
      maxNbItem--;
    } else {
      break;
    }
  }
    // Search for sci names
  for (map<wstring,int>::const_iterator
       it(sci_names_index_i18n.lower_bound(objw));
       it!=sci_names_index_i18n.end();it++) {
    const wstring constw(it->first.substr(0,objw.size()));
    if (constw == objw) {
      if (maxNbItem == 0) break;
      result.push_back(getSciName(it->second));
      maxNbItem--;
    } else {
      break;
    }
  }

  sort(result.begin(), result.end());

  return result;
}


//! Define font file name and size to use for star names display
void StarMgr::setFontSize(double newFontSize) {
  fontSize = newFontSize;
  starFont = &StelApp::getInstance().getFontManager().getStandardFont(
               StelApp::getInstance().getLocaleMgr().getSkyLanguage(),
               fontSize);
}

void StarMgr::updateSkyCulture(LoadingBar& lb)
{
	string skyCultureDir = StelApp::getInstance().getSkyCultureMgr().getSkyCultureDir();
	
	// Load culture star names in english
	try
	{
		load_common_names(StelApp::getInstance().getFileMgr().findFile("skycultures/" + skyCultureDir + "/star_names.fab"));
	}
	catch(exception& e)
	{
		cout << "WARNING: could not load star_names.fab for sky culture " 
			<< skyCultureDir << ": " << e.what() << endl;	
	}
	
	try
	{
		load_sci_names(StelApp::getInstance().getFileMgr().findFile("stars/default/name.fab"));
	}
	catch(exception& e)
	{
		cout << "WARNING: could not load scientific star names file: " << e.what() << endl;	
	}

	// Turn on sci names/catalog names for western culture only
	setFlagSciNames( skyCultureDir.compare(0, 7, "western") ==0 );
	updateI18n();
}
