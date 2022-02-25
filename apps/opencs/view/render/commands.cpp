#include "commands.hpp"

#include <components/debug/debuglog.hpp>

#include "terrainshapemode.hpp"
#include "worldspacewidget.hpp"

CSVRender::DrawTerrainSelectionCommand::DrawTerrainSelectionCommand(WorldspaceWidget* worldspaceWidget, QUndoCommand* parent)
    : mWorldspaceWidget(worldspaceWidget)
{ }

void CSVRender::DrawTerrainSelectionCommand::redo()
{
    tryUpdate();
}

void CSVRender::DrawTerrainSelectionCommand::undo()
{
    tryUpdate();
}

void CSVRender::DrawTerrainSelectionCommand::tryUpdate()
{
    if (!mWorldspaceWidget)
    {
        Log(Debug::Verbose) << "Can't update terrain selection, no WorldspaceWidget found!";
        return;
    }

    auto terrainMode = dynamic_cast<CSVRender::TerrainShapeMode*>(mWorldspaceWidget->getEditMode());
    if (!terrainMode)
    {
        Log(Debug::Verbose) << "Can't update terrain selection in current EditMode";
        return;
    }

    terrainMode->getTerrainSelection()->update();
}
