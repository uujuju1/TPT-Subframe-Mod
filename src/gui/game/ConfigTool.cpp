#include "Tool.h"

#include "graphics/Graphics.h"
#include "graphics/Renderer.h"

#include "gui/interface/Window.h"
#include "gui/interface/Button.h"

#include "gui/game/Brush.h"

#include "simulation/ElementClasses.h"
#include "simulation/Simulation.h"

class FiltConfigWindow: public ui::Window
{
public:
	ConfigTool *tool;
	Simulation *sim;
	FiltConfigWindow(ConfigTool *tool_, Simulation *sim);
	void OnSelect(int result);
	virtual void OnTryExit(ExitMethod method);
	virtual ~FiltConfigWindow() {}
};

FiltConfigWindow::FiltConfigWindow(ConfigTool * tool_, Simulation *sim_):
ui::Window(ui::Point(-1, -1), ui::Point(150, 200)),
tool(tool_),
sim(sim_)
{
	int maxTextWidth = 0;
	for (int i = 0; i <= FILT_NUM_MODES; i++)
	{
		String buttonText = (i == FILT_NUM_MODES) ?
			String::Build("Cancel") : FILT_MODES[i];
		int textWidth = Graphics::textwidth(buttonText);
		if (textWidth > maxTextWidth)
			maxTextWidth = textWidth;
	}
	int buttonWidth = maxTextWidth + 15;
	int buttonHeight = 17;
	int buttonLeft = Size.X/2 - buttonWidth/2;
	int buttonTop = Size.Y/2 - ((buttonHeight-1) * (FILT_NUM_MODES))/2;
	for (int i = 0; i < FILT_NUM_MODES; i++)
	{
		String buttonText = FILT_MODES[i];
		ui::Button * b = new ui::Button(ui::Point(buttonLeft, i * (buttonHeight-1) + buttonTop), ui::Point(buttonWidth, buttonHeight), buttonText);
		b->Appearance.HorizontalAlign = ui::Appearance::AlignCentre;
		b->Appearance.VerticalAlign = ui::Appearance::AlignMiddle;
		b->SetActionCallback({ [this, i] { this->OnSelect(i); } });
		AddComponent(b);
	}
	MakeActiveWindow();
}

void FiltConfigWindow::OnSelect(int result)
{
	tool->OnSelectFiltTmp(sim, result);
	CloseActiveWindow();
	SelfDestruct();
}

void FiltConfigWindow::OnTryExit(ExitMethod method)
{
	CloseActiveWindow();
	SelfDestruct();
}

int ConfigTool::getIdAt(Simulation *sim, ui::Point position)
{
	if (position.X<0 || position.X>=XRES || position.Y<0 || position.Y>=YRES)
		return -1;
	int p = sim->photons[position.Y][position.X];
	if (!p)
	{
		p = sim->pmap[position.Y][position.X];
		if (!p)
			return -1;
	}
	return ID(p);
}

Particle ConfigTool::getPartAt(Simulation *sim, ui::Point position)
{
	int id = getIdAt(sim, position);
	Particle nullPart;
	nullPart.type = PT_NONE;
	return (id == -1) ? nullPart : sim->parts[id];
}

bool ConfigTool::isSamePart(Particle p1, Particle p2)
{
	return p1.type == p2.type &&
		int(p1.x+0.5f) == int(p2.x+0.5f) &&
		int(p1.y+0.5f) == int(p2.y+0.5f);
}

ui::Point ConfigTool::projectPoint(Particle part, int sampleX, int sampleY, bool allowDiag)
{
	int partX = int(part.x + 0.5f), partY = int(part.y + 0.5f);
	int relX = sampleX - partX, relY = sampleY - partY;
	int absX = (relX > 0) ? relX : -relX, absY = (relY > 0) ? relY : -relY;
	int projX = 0, projY = 0, diagProjX, diagProjY;
	if (absX > absY)
		projX = relX;
	else
		projY = relY;
	if ((relX > 0) == (relY > 0))
	{
		diagProjX = (relX + relY) / 2;
		diagProjY = diagProjX;
	}
	else
	{
		diagProjX = (relX - relY) / 2;
		diagProjY = -diagProjX;
	}
	int relProjX = relX - projX, relProjY = relY - projY;
	int relDiagProjX = relX - diagProjX, relDiagProjY = relY - diagProjY;
	int distProj = relProjX*relProjX + relProjY*relProjY;
	int distDiagProj = relDiagProjX*relDiagProjX + relDiagProjY*relDiagProjY;
	if (distProj < distDiagProj || !allowDiag)
		return ui::Point(projX, projY);
	else
		return ui::Point(diagProjX, diagProjY);
}

int ConfigTool::getDist(ui::Point relPos, int offset)
{
	int signedDist = relPos.X ? relPos.X : relPos.Y;
	int dist = ((signedDist > 0) ? signedDist : -signedDist) - offset;
	return (dist < 0) ? 0 : dist;
}

int ConfigTool::getDist(Particle part, int sampleX, int sampleY, int offset, bool allowDiag)
{
	ui::Point proj = projectPoint(part, sampleX, sampleY, allowDiag);
	return getDist(proj, offset);
}

int ConfigTool::getTargetStackEditDepth(SimulationSample *sample)
{
	for (int i = sample->EffectiveStackEditDepth; i < sample->StackIndexEnd; i++)
	{
		int stacki = i - sample->StackIndexBegin;
		if (IsConfigurableType(sample->SParticles[stacki].type))
			return stacki + sample->StackIndexBegin;
	}
	return -1;
}

void ConfigTool::OnSelectFiltTmp(Simulation *sim, int tmp)
{
	if (IsCorrupted(sim))
	{
		Reset(sim);
		return;
	}
	sim->parts[configPartId].tmp = tmp;
}

void ConfigTool::Update(Simulation *sim)
{
	SimulationSample *sample = &sim->sample;
	if (IsCorrupted(sim))
		Reset(sim);
	cursorPos = ui::Point(sample->PositionX, sample->PositionY);
	bool allowDiag = !(
		configState == ConfigState::dtecTmp2 ||
		configState == ConfigState::tsnsTmp2 ||
		configState == ConfigState::lsnsTmp2
	);
	ui::Point proj(0, 0);
	if (configState != ConfigState::ready)
	{
		proj = projectPoint(configPart, cursorPos.X, cursorPos.Y, allowDiag);
		dirx = (proj.X == 0) ? 0 : ((proj.X > 0) ? 1 : -1);
		diry = (proj.Y == 0) ? 0 : ((proj.Y > 0) ? 1 : -1);
	}
	switch (configState)
	{
	case ConfigState::ready: {
		configPartId = -1;
		if (!sample->isMouseInSim)
			break;
		memcpy(lastAdjacentPartsInfo, sample->AdjacentPartsInfo, sizeof(sample->AdjacentPartsInfo));
		int targetDepth = getTargetStackEditDepth(sample);
		if (targetDepth == -1)
			configPartId = -1;
		else
		{
			int stacki = targetDepth - sample->StackIndexBegin;
			configPartId = sample->SParticleIDs[stacki];
			configPart = sample->SParticles[stacki];
		}
		break;
	}
	case ConfigState::drayTmp:
		configPart.tmp = getDist(proj);
		break;
	case ConfigState::drayTmp2:
		configPart.tmp2 = getDist(proj, configPart.tmp);
		break;
	case ConfigState::crayTmp2:
		configPart.tmp2 = getDist(proj);
		break;
	case ConfigState::crayTmp:
		configPart.tmp = getDist(proj, configPart.tmp2);
		break;
	case ConfigState::ldtcLife:
		configPart.life = getDist(proj);
		break;
	case ConfigState::ldtcTmp:
		configPart.tmp = getDist(proj, configPart.life);
		break;
	case ConfigState::dtecTmp2:
	case ConfigState::tsnsTmp2:
	case ConfigState::lsnsTmp2:
		configPart.tmp2 = getDist(proj);
		break;
	case ConfigState::convTmp:
		configPart.tmp = sample->isMouseInSim ? sample->particle.type : 0;
		break;
	default:
		break;
	}
}

void ConfigTool::Click(Simulation *sim, Brush *brush, ui::Point position)
{
	sim->UpdateSample(position.X, position.Y);
	Update(sim);
	if (IsCorrupted(sim))
	{
		Reset(sim);
		return;
	}
	Particle *pConfigPart = NULL;
	if (configState != ConfigState::ready)
		pConfigPart = &sim->parts[configPartId];
	switch (configState)
	{
	case ConfigState::ready:
		if (configPartId == -1)
			break;
		switch (configPart.type)
		{
		case PT_DRAY:
			configState = ConfigState::drayTmp;
			break;
		case PT_CRAY:
			configState = ConfigState::crayTmp2;
			break;
		case PT_LDTC:
			configState = ConfigState::ldtcLife;
			break;
		case PT_DTEC:
			configState = ConfigState::dtecTmp2;
			break;
		case PT_TSNS:
			configState = ConfigState::tsnsTmp2;
			break;
		case PT_LSNS:
			configState = ConfigState::lsnsTmp2;
			break;
		case PT_CONV:
			configState = ConfigState::convTmp;
			break;
		case PT_FILT:
			new FiltConfigWindow(this, sim);
			break;
		default:
			break;
		}
		if (configState != ConfigState::ready)
		{
			sim->configToolSampleActive = true;
			sim->configToolSampleX = cursorPos.X;
			sim->configToolSampleY = cursorPos.Y;
		}
		break;
	case ConfigState::drayTmp:
		pConfigPart->tmp = configPart.tmp;
		configState = ConfigState::drayTmp2;
		break;
	case ConfigState::drayTmp2:
		pConfigPart->tmp2 = configPart.tmp2;
		Reset(sim);
		break;
	case ConfigState::crayTmp2:
		pConfigPart->tmp2 = configPart.tmp2;
		configState = ConfigState::crayTmp;
		break;
	case ConfigState::crayTmp:
		pConfigPart->tmp = configPart.tmp;
		Reset(sim);
		break;
	case ConfigState::ldtcLife:
		pConfigPart->life = configPart.life;
		configState = ConfigState::ldtcTmp;
		break;
	case ConfigState::ldtcTmp:
		pConfigPart->tmp = configPart.tmp;
		Reset(sim);
		break;
	case ConfigState::dtecTmp2:
	case ConfigState::tsnsTmp2:
	case ConfigState::lsnsTmp2:
		pConfigPart->tmp2 = configPart.tmp2;
		Reset(sim);
		break;
	case ConfigState::convTmp:
		pConfigPart->tmp = configPart.tmp;
		Reset(sim);
		break;
	default:
		break;
	}
}

bool ConfigTool::IsCorrupted(Simulation *sim)
{
	if (configState == ConfigState::ready)
		return false;
	if (!isSamePart(sim->parts[configPartId], configPart))
		return true;
	int targetDepth = getTargetStackEditDepth(&sim->sample);
	if (targetDepth == -1)
		return configPartId != -1;
	int stacki = targetDepth - sim->sample.StackIndexBegin;
	return sim->sample.SParticleIDs[stacki] != configPartId;
}

void ConfigTool::Reset(Simulation *sim)
{
	configState = ConfigState::ready;
	sim->configToolSampleActive = false;
}

Particle ConfigTool::GetPart()
{
	return configPart;
}

int ConfigTool::GetId()
{
	return configPartId;
}

bool ConfigTool::IsConfigurableType(int type)
{
	return type == PT_DRAY || type == PT_CRAY || type == PT_LDTC ||
		type == PT_DTEC || type == PT_TSNS || type == PT_LSNS ||
		type == PT_CONV || type == PT_FILT;
}

bool ConfigTool::IsConfiguring()
{
	return configState != ConfigState::ready;
}

bool ConfigTool::IsConfiguringTemp()
{
	return false;
}

bool ConfigTool::IsConfiguringLife()
{
	return configState == ConfigState::ldtcLife;
}

bool ConfigTool::IsConfiguringTmp()
{
	return configState == ConfigState::drayTmp ||
		configState == ConfigState::crayTmp ||
		configState == ConfigState::ldtcTmp ||
		configState == ConfigState::convTmp;
}

bool ConfigTool::IsConfiguringTmp2()
{
	return configState == ConfigState::drayTmp2 ||
		configState == ConfigState::crayTmp2 ||
		configState == ConfigState::dtecTmp2 ||
		configState == ConfigState::tsnsTmp2 ||
		configState == ConfigState::lsnsTmp2;
}

void ConfigTool::drawRedLine(Renderer *ren, int startx, int starty, int endx, int endy)
{
	ren->draw_line(startx, starty, endx, endy, 255, 0, 0, 200);
}

void ConfigTool::drawWhiteLine(Renderer *ren, int startx, int starty, int endx, int endy)
{
	ren->draw_line(startx, starty, endx, endy, 255, 200, 200, 220);
}

void ConfigTool::drawTripleLine(Renderer *ren, int firstLineLen, int midLineLen, bool drawFirstLine, bool drawThirdLine)
{
	int mid1x = configPart.x + dirx * firstLineLen,
		mid1y = configPart.y + diry * firstLineLen;
	int mid2x = mid1x + dirx * midLineLen,
		mid2y = mid1y + diry * midLineLen;
	if (drawFirstLine && firstLineLen > 0)
		drawWhiteLine(ren, configPart.x + dirx, configPart.y + diry, mid1x, mid1y);
	if (midLineLen > 0)
		drawRedLine(ren, mid1x + dirx, mid1y + diry, mid2x, mid2y);
	if (drawThirdLine && firstLineLen > 0)
		drawWhiteLine(ren, mid2x + dirx, mid2y + diry, mid2x + dirx * firstLineLen, mid2y + diry * firstLineLen);
}

void ConfigTool::drawSquareRdBox(Renderer *ren)
{
	ren->drawrect(configPart.x - configPart.tmp2, configPart.y - configPart.tmp2, configPart.tmp2 * 2 + 1, configPart.tmp2 * 2 + 1, 200, 200, 200, 220);
}

void ConfigTool::DrawHUD(Renderer *ren)
{
	switch (configState)
	{
	case ConfigState::ready:
		ren->xor_line(cursorPos.X, cursorPos.Y, cursorPos.X, cursorPos.Y);
		if (configPartId == -1)
			break;
		switch (configPart.type)
		{
		case PT_DTEC:
		case PT_TSNS:
		case PT_LSNS:
			drawSquareRdBox(ren);
			break;
		case PT_DRAY:
		case PT_CRAY:
		case PT_LDTC:
			for (diry = -1; diry <= 1; diry++)
			{
				for (dirx = -1; dirx <= 1; dirx++)
				{
					if (!(dirx || diry))
						continue;
					bool hasConductor =
						(lastAdjacentPartsInfo[-diry+1][-dirx+1] &
						SimulationSample::SPRK_FLAG) != 0;
					bool hasFilt =
						(lastAdjacentPartsInfo[-diry+1][-dirx+1] &
						SimulationSample::FILT_FLAG) != 0;
					if (!hasConductor &&
						!(configPart.type == PT_LDTC && hasFilt))
						continue;
					switch (configPart.type)
					{
					case PT_DRAY:
						drawTripleLine(ren, configPart.tmp, configPart.tmp2);
						break;
					case PT_CRAY:
						drawTripleLine(ren, configPart.tmp2, configPart.tmp, true, false);
						break;
					case PT_LDTC:
						drawTripleLine(ren, configPart.life, configPart.tmp, true, false);
						break;
					}
				}
			}
			break;
		}
		break;
	case ConfigState::drayTmp:
		drawTripleLine(ren, 0, configPart.tmp, false, false);
		break;
	case ConfigState::drayTmp2:
		drawTripleLine(ren, configPart.tmp, configPart.tmp2);
		break;
	case ConfigState::crayTmp2:
		drawTripleLine(ren, 0, configPart.tmp2, false, false);
		break;
	case ConfigState::crayTmp:
		drawTripleLine(ren, configPart.tmp2, configPart.tmp, true, false);
		break;
	case ConfigState::ldtcLife:
		drawTripleLine(ren, 0, configPart.life, false, false);
		break;
	case ConfigState::ldtcTmp:
		drawTripleLine(ren, configPart.life, configPart.tmp, true, false);
		break;
	case ConfigState::dtecTmp2:
	case ConfigState::tsnsTmp2:
	case ConfigState::lsnsTmp2:
		drawSquareRdBox(ren);
		break;
	case ConfigState::convTmp:
	{
		int partX = int(configPart.x+0.5f), partY = int(configPart.y+0.5f);
		drawRedLine(ren, partX, partY, partX, partY);
		ren->xor_line(cursorPos.X, cursorPos.Y, cursorPos.X, cursorPos.Y);
		break;
	}
	default:
		break;
	}
}

void ConfigTool::ReleaseTool::Click(Simulation *sim, Brush *brush, ui::Point position)
{
	if (configTool->IsConfiguring())
		configTool->Reset(sim);
	else
	{
		ui::Point oldSize = brush->GetRadius();
		brush->SetRadius(ui::Point(0, 0));
		clearTool->Click(sim, brush, position);
		brush->SetRadius(oldSize);
	}
}

void ConfigTool::ReleaseTool::Draw(Simulation * sim, Brush * brush, ui::Point position)
{
	if (!configTool->IsConfiguring())
	{
		ui::Point oldSize = brush->GetRadius();
		brush->SetRadius(ui::Point(0, 0));
		clearTool->Draw(sim, brush, position);
		brush->SetRadius(oldSize);
	}
}

void ConfigTool::ReleaseTool::DrawLine(Simulation * sim, Brush * brush, ui::Point position1, ui::Point position2, bool dragging)
{
	if (!configTool->IsConfiguring())
	{
		ui::Point oldSize = brush->GetRadius();
		brush->SetRadius(ui::Point(0, 0));
		clearTool->DrawLine(sim, brush, position1, position2, dragging);
		brush->SetRadius(oldSize);
	}
}

void ConfigTool::ReleaseTool::DrawRect(Simulation * sim, Brush * brush, ui::Point position1, ui::Point position2)
{
	if (!configTool->IsConfiguring())
	{
		ui::Point oldSize = brush->GetRadius();
		brush->SetRadius(ui::Point(0, 0));
		clearTool->DrawRect(sim, brush, position1, position2);
		brush->SetRadius(oldSize);
	}
}

void ConfigTool::ReleaseTool::DrawFill(Simulation * sim, Brush * brush, ui::Point position)
{
	if (!configTool->IsConfiguring())
	{
		ui::Point oldSize = brush->GetRadius();
		brush->SetRadius(ui::Point(0, 0));
		clearTool->DrawFill(sim, brush, position);
		brush->SetRadius(oldSize);
	}
}
