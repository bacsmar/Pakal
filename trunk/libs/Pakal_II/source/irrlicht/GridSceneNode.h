#ifndef __C_GRID_SCENE_NODE_H__
#define __C_GRID_SCENE_NODE_H__
 
#include "irrlicht.h"
 
//! Grid scene node
/*! If you need a grid on the XY or ZY axis, simply rotate this node by 90
degrees in the appropiate axis.
This node creates an XZ grid by default, which should be fine for normal use.
Axis Lines are a default Red and Blue for the X and Z axis respectively.
 
Please note that the internal meshbuffer used for the grid has a max size of 65535 indices.
 
Thanks goes to MasterGod for helping to clean up the code and for a few bug fixes.
 
Additional thanks to:
JP for optimising the rendering.
Vins for fixing a nasty crash bug and optimising memory usage.
*/
 
class CGridSceneNode : public irr::scene::ISceneNode
{
public:
    //! Constructor
    CGridSceneNode(irr::scene::ISceneNode* parent, irr::scene::ISceneManager* smgr, irr::s32 id = -1,
        irr::u32 spacing = 8, irr::u32 size = 1024, irr::video::SColor gridcolor = irr::video::SColor(255,128,128,128),
        irr::u32 accentlineoffset = 8, irr::video::SColor accentgridcolor = irr::video::SColor(255,192,192,192),
        bool axislinestate = false);
        
    virtual ~CGridSceneNode();
 
    //! Will create a copy of this scenenode and it's settings
	CGridSceneNode* clone(irr::scene::ISceneNode* newParent = nullptr, irr::scene::ISceneManager* newSceneManager = nullptr) override;
 
    //! Pre-Register stuff
	void OnRegisterSceneNode() override;
 
    //! Render our grid using 3D lines stored inside the internal meshbuffer
	void render() override;
 
    //! Returns our bounding box
	const irr::core::aabbox3d<irr::f32>& getBoundingBox() const override;
 
    //! Returns the total number of materials, in this case, only 1
	irr::u32 getMaterialCount() const override;
 
    //! Returns the only material
	irr::video::SMaterial& getMaterial(irr::u32 i) override;
 
    //! Will cause the grid scene node to rebuild it's grid
    void RegenerateGrid();
 
    //! Returns the Spacing of the grid
    irr::u32 GetSpacing();
 
    //! Returns the total size of the grid
    irr::u32 GetSize();
 
    //! Returns the Grid Color
    irr::video::SColor GetGridColor();
 
    //! Returns the offset of the accent lines
    irr::u32 GetAccentlineOffset();
 
    //! Returns the Accent Line Color
    irr::video::SColor GetAccentlineColor();
 
    //! Returns the Active State of the Axis Lines
    bool AreAxisLineActive();
 
    //! Returns the Color of the "X" axis lines
    irr::video::SColor GetAxisLineXColor();
 
    //! Returns the Color of the "Z" axis lines
    irr::video::SColor GetAxisLineZColor();
 
    //! Sets Spacing
    void SetSpacing(irr::u32 newspacing);
 
    //! Sets Size
    void SetSize(irr::u32 newsize);
 
    //! Sets the general grid color
    void SetGridColor(irr::video::SColor newcolor);
 
    //! Sets the offset for the accent lines
    //! If > 0, accent lines will be active, otherwise not
    void SetAccentlineOffset(irr::u32 newoffset);
 
    //! Sets the color of the accent lines
    void SetAccentlineColor(irr::video::SColor newcolor);
 
    //! Sets whether the lines denoting the center of the grid are active
    void SetAxisLineActive(bool active);
 
    //! Sets the Color of the "X" axis lines
    void SetAxisLineXColor(irr::video::SColor XLine);
   
    //! Sets the Color of the "Z" axis lines
    void SetAxisLineZColor(irr::video::SColor ZLine);
 
    //! Allows for setting a complete new material
    void SetMaterial(irr::video::SMaterial newMaterial);
 
private:
    irr::u32 m_spacing;
    irr::u32 m_size;
    irr::video::SColor m_gridcolor;
    irr::video::SColor m_accentgridcolor;
    irr::u32 m_accentlineoffset;
    bool m_AxisLineState;
    irr::video::SColor m_XLineColor;
    irr::video::SColor m_YLineColor;
    irr::video::SColor m_ZLineColor;
 
    irr::scene::IDynamicMeshBuffer * m_buffer;
};
 
#endif // __C_GRID_SCENE_NODE_H__