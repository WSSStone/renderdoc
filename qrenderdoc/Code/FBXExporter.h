/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include <QString>
#include <QVector>
#include "renderdoc_replay.h"

namespace FBXExporter
{
struct Attribute
{
  QString name;
  int components = 0;
  QVector<double> values;
};
struct Mesh
{
  QVector<double> positions;
  QVector<uint32_t> triangles;
  Attribute normal, tangent, color;
  Attribute uv[3];
};
// Input rows refer to expanded vertices; UINT32_MAX denotes primitive restart.
bool Triangulate(Topology topology, const QVector<uint32_t> &rows, QVector<uint32_t> &triangles,
                 QString &error);
bool ResolveIndex(uint32_t raw, int32_t baseVertex, uint32_t restart, uint32_t &vertex,
                  bool &isRestart, QString &error);
bool Write(const QString &filename, const Mesh &mesh, QString &error);
}
