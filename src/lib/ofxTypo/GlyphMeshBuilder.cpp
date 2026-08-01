#include "GlyphMeshBuilder.h"

namespace {

void appendCommand(ofPath &path, const PathCommand &command, glm::vec2 &curPos) {
    switch (command.type) {
    case PathCommandType::MoveTo:
        path.moveTo(command.p0.x, -command.p0.y);
        curPos = {command.p0.x, -command.p0.y};
        break;
    case PathCommandType::LineTo:
        path.lineTo(command.p0.x, -command.p0.y);
        curPos = {command.p0.x, -command.p0.y};
        break;
    case PathCommandType::QuadTo: {
        glm::vec2 ctrl = {command.p0.x, -command.p0.y};
        glm::vec2 end  = {command.p1.x, -command.p1.y};
        // ofPolyline::quadBezierTo(p0, ctrl, p1) treats the first arg as the
        // explicit start point of the curve, so we must pass the current position.
        path.quadBezierTo(curPos.x, curPos.y, ctrl.x, ctrl.y, end.x, end.y);
        curPos = end;
        break;
    }
    case PathCommandType::CubicTo:
        path.bezierTo(command.p0.x, -command.p0.y, command.p1.x, -command.p1.y, command.p2.x, -command.p2.y);
        curPos = {command.p2.x, -command.p2.y};
        break;
    case PathCommandType::Close:
        path.close();
        break;
    }
}

} // namespace

bool GlyphMeshBuilder::buildFillMesh(const GlyphOutline &outline, ofMesh &outMesh) const {
    outMesh.clear();
    if (outline.empty()) {
        return false;
    }

    ofPath path;
    path.setFilled(true);
    path.setPolyWindingMode(OF_POLY_WINDING_ODD);
    path.setCurveResolution(64);

    for (const auto &contour : outline.contours) {
        glm::vec2 curPos = {0.0f, 0.0f};
        for (const auto &command : contour.commands) {
            appendCommand(path, command, curPos);
        }
    }

    outMesh = path.getTessellation();
    return !outMesh.getVertices().empty() && !outMesh.getIndices().empty();
}
