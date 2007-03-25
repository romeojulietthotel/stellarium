/*
 * Stellarium
 * Copyright (C) 2003 Fabien Chereau
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

#ifndef _LANDSCAPE_H_
#define _LANDSCAPE_H_

#include <string>
#include "vecmath.h"
#include "ToneReproducer.hpp"
#include "Projector.hpp"
#include "Navigator.hpp"
#include "Fader.hpp"
#include "StelUtils.hpp"
#include "STextureTypes.hpp"

// Class which manages the displaying of the Landscape
class Landscape
{
public:
	enum LANDSCAPE_TYPE
	{
		OLD_STYLE,
		FISHEYE,
		SPHERICAL
	};

	Landscape(float _radius = 2.);
	virtual ~Landscape();
	virtual void load(const string& file_name, const string& section_name) = 0;
	void set_sky_brightness(float b) {sky_brightness = b;}
	
	//! Set whether landscape is displayed (does not concern fog)
	void setFlagShow(bool b) {land_fader=b;}
	//! Get whether landscape is displayed (does not concern fog)
	bool getFlagShow() const {return (bool)land_fader;}
	//! Set whether fog is displayed
	void setFlagShowFog(bool b) {fog_fader=b;}
	//! Get whether fog is displayed
	bool getFlagShowFog() const {return (bool)fog_fader;}
	//! Get landscape name
	wstring getName() const {return name;}
	//! Get landscape author name
	wstring getAuthorName() const {return author;}
	//! Get landscape description
	wstring getDescription() const {return description;}
	
	void update(double deltaTime) {land_fader.update((int)(deltaTime*1000)); fog_fader.update((int)(deltaTime*1000));}
	virtual void draw(ToneReproducer * eye, const Projector* prj, const Navigator* nav) = 0;

protected:
	//! Load attributes common to all landscapes
	void loadCommon(const string& landscape_file, const string& section_name);
	
	//! search for a texture in landscape directory, else global textures directory
	//! @param basename The name of a texture file, e.g. "fog.png"
	//! @param landscapeName The landscape ID (directory name) to which the texture belongs
	//! @exception misc possibility of throwing boost:filesystem or "file not found" exceptions
	const string getTexturePath(const string& basename, const string& landscapeName);
	float radius;
	wstring name;
	float sky_brightness;
	bool valid_landscape;   // was a landscape loaded properly?
	LinearFader land_fader;
	LinearFader fog_fader;
	wstring author;
	wstring description;
	
typedef struct
{
	STextureSP tex;
	float tex_coords[4];
} landscape_tex_coord;
};


class LandscapeOldStyle : public Landscape
{
public:
	LandscapeOldStyle(float _radius = 2.);
    virtual ~LandscapeOldStyle();
	virtual void load(const string& fileName, const string& section_name);
	virtual void draw(ToneReproducer * eye, const Projector* prj, const Navigator* nav);
	void create(bool _fullpath, stringHash_t param);
private:
	void draw_fog(ToneReproducer * eye, const Projector* prj, const Navigator* nav) const;
	void draw_decor(ToneReproducer * eye, const Projector* prj, const Navigator* nav) const;
	void draw_ground(ToneReproducer * eye, const Projector* prj, const Navigator* nav) const;
	STextureSP* side_texs;
	int nb_side_texs;
	int nb_side;
	landscape_tex_coord* sides;
	STextureSP fog_tex;
	landscape_tex_coord fog_tex_coord;
	STextureSP ground_tex;
	landscape_tex_coord ground_tex_coord;
	int nb_decor_repeat;
	float fog_alt_angle;
	float fog_angle_shift;
	float decor_alt_angle;
	float decor_angle_shift;
	float decor_angle_rotatez;
	float ground_angle_shift;
	float ground_angle_rotatez;
	int draw_ground_first;
};

class LandscapeFisheye : public Landscape
{
public:
	LandscapeFisheye(float _radius = 1.);
	virtual ~LandscapeFisheye();
	virtual void load(const string& fileName, const string& section_name);
	virtual void draw(ToneReproducer * eye, const Projector* prj, const Navigator* nav);
	void create(const wstring _name, bool _fullpath, const string _maptex, double _texturefov);
private:

	STextureSP map_tex;
	float tex_fov;
};


class LandscapeSpherical : public Landscape
{
public:
	LandscapeSpherical(float _radius = 1.);
	virtual ~LandscapeSpherical();
	virtual void load(const string& fileName, const string& section_name);
	virtual void draw(ToneReproducer * eye, const Projector* prj, const Navigator* nav);
	void create(const wstring _name, bool _fullpath, const string _maptex);
private:

	STextureSP map_tex;
};

#endif // _LANDSCAPE_H_
