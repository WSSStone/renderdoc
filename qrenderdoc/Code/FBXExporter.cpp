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

#include "FBXExporter.h"
#include <QLocale>
#include <QSaveFile>
#include <QTextStream>
#include <cmath>
#include <limits>
#include "Code/QRDUtils.h"

namespace FBXExporter
{
bool ResolveIndex(uint32_t raw, int32_t baseVertex, uint32_t restart, uint32_t &vertex,
                  bool &isRestart, QString &error)
{
  isRestart = restart != 0 && raw == restart;
  if(isRestart)
    return true;
  int64_t result = int64_t(raw) + baseVertex;
  if(result < 0 || result >= int64_t(UINT32_MAX))
  {
    error = QStringLiteral("Index plus base vertex is out of range.");
    return false;
  }
  vertex = uint32_t(result);
  return true;
}

bool Triangulate(Topology topology, const QVector<uint32_t> &rows, QVector<uint32_t> &triangles,
                 QString &error)
{
  triangles.clear();
  if(topology != Topology::TriangleList && topology != Topology::TriangleStrip &&
     topology != Topology::TriangleFan)
  {
    error = QStringLiteral("FBX export supports triangle lists, strips and fans only.");
    return false;
  }
  QVector<uint32_t> primitive;
  for(uint32_t row : rows)
  {
    if(row == UINT32_MAX)
    {
      primitive.clear();
      continue;
    }
    primitive.push_back(row);
    int n = primitive.size();
    if(n < 3)
      continue;
    if(topology == Topology::TriangleList)
    {
      triangles << primitive[0] << primitive[1] << primitive[2];
      primitive.clear();
    }
    else if(topology == Topology::TriangleStrip)
    {
      triangles << primitive[n - (n % 2 ? 3 : 2)] << primitive[n - (n % 2 ? 2 : 3)]
                << primitive[n - 1];
    }
    else
    {
      triangles << primitive[0] << primitive[n - 2] << primitive[n - 1];
    }
  }
  if(triangles.isEmpty())
  {
    error = QStringLiteral("The selected mesh contains no complete triangles.");
    return false;
  }
  return true;
}

static bool CheckAttribute(const Attribute &attr, int vertices, int components)
{
  if(attr.values.isEmpty())
    return true;
  if(attr.components != components || attr.values.size() != vertices * components)
    return false;
  for(double value : attr.values)
    if(!std::isfinite(value))
      return false;
  return true;
}

static void Array(QTextStream &s, const QString &name, const QVector<double> &values)
{
  s << "        " << name << ": *" << values.size() << " {\n            a: ";
  for(int i = 0; i < values.size(); i++)
  {
    if(i)
      s << ",";
    if(i && i % 24 == 0)
      s << "\n            ";
    s << values[i];
  }
  s << "\n        }\n";
}

static QString Quoted(QString value)
{
  value.replace(QLatin1Char('\\'), QLatin1Char('_'));
  value.replace(QLatin1Char('"'), QLatin1Char('_'));
  value.replace(QLatin1Char('\n'), QLatin1Char('_'));
  value.replace(QLatin1Char('\r'), QLatin1Char('_'));
  return value;
}

static void Layer(QTextStream &s, const char *type, const char *array, const Attribute &attr,
                  int index)
{
  if(attr.values.isEmpty())
    return;
  s << "    LayerElement" << type << ": " << index << " {\n"
    << "        Version: 101\n        Name: \"" << Quoted(attr.name) << "\"\n"
    << "        MappingInformationType: \"ByVertice\"\n"
    << "        ReferenceInformationType: \"Direct\"\n";
  Array(s, QString::fromLatin1(array), attr.values);
  s << "    }\n";
}

bool Write(const QString &filename, const Mesh &mesh, QString &error)
{
  const int vertices = mesh.positions.size() / 3;
  if(vertices == 0 || mesh.positions.size() % 3 || mesh.triangles.isEmpty() ||
     mesh.triangles.size() % 3)
  {
    error = QStringLiteral("Invalid or empty mesh.");
    return false;
  }
  for(double value : mesh.positions)
    if(!std::isfinite(value))
    {
      error = QStringLiteral("Position contains a non-finite value.");
      return false;
    }
  for(uint32_t index : mesh.triangles)
    if(index >= uint32_t(vertices))
    {
      error = QStringLiteral("Triangle index exceeds the vertex count.");
      return false;
    }
  if(!CheckAttribute(mesh.normal, vertices, 3) || !CheckAttribute(mesh.tangent, vertices, 3) ||
     !CheckAttribute(mesh.color, vertices, 4) || !CheckAttribute(mesh.uv[0], vertices, 2) ||
     !CheckAttribute(mesh.uv[1], vertices, 2) || !CheckAttribute(mesh.uv[2], vertices, 2))
  {
    error = QStringLiteral("Invalid attribute size or non-finite attribute value.");
    return false;
  }
  QSaveFile file(filename);
  if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
  {
    error = file.errorString();
    return false;
  }
  QTextStream s(&file);
  s.setCodec("UTF-8");
  s.setLocale(QLocale::c());
  s.setRealNumberPrecision(17);
  s << "; FBX 7.4.0 project file\n"
    << "FBXHeaderExtension: {\n    FBXHeaderVersion: 1003\n    FBXVersion: 7400\n"
    << "    Creator: \"RenderDoc FBX Exporter\"\n}\n"
    << "GlobalSettings: {\n    Version: 1000\n    Properties70: {\n"
    << "        P: \"UpAxis\", \"int\", \"Integer\", \"\",1\n"
    << "        P: \"UpAxisSign\", \"int\", \"Integer\", \"\",1\n"
    << "        P: \"FrontAxis\", \"int\", \"Integer\", \"\",2\n"
    << "        P: \"FrontAxisSign\", \"int\", \"Integer\", \"\",-1\n"
    << "        P: \"CoordAxis\", \"int\", \"Integer\", \"\",0\n"
    << "        P: \"CoordAxisSign\", \"int\", \"Integer\", \"\",1\n"
    << "        P: \"UnitScaleFactor\", \"double\", \"Number\", \"\",1\n    }\n}\n"
    << "Documents: {\n    Count: 1\n    Document: 1, \"Scene\", \"Scene\" {\n"
    << "        RootNode: 0\n    }\n}\n"
    << "Definitions: {\n    Version: 100\n    Count: 2\n"
    << "    ObjectType: \"Geometry\" { Count: 1 }\n"
    << "    ObjectType: \"Model\" { Count: 1 }\n}\n"
    << "Objects: {\n  Geometry: 2, \"Geometry::Mesh\", \"Mesh\" {\n"
    << "    GeometryVersion: 124\n";
  Array(s, QStringLiteral("Vertices"), mesh.positions);
  s << "    PolygonVertexIndex: *" << mesh.triangles.size() << " {\n        a: ";
  for(int i = 0; i < mesh.triangles.size(); i++)
  {
    if(i)
      s << ",";
    if(i && i % 24 == 0)
      s << "\n        ";
    s << (i % 3 == 2 ? -int64_t(mesh.triangles[i]) - 1 : int64_t(mesh.triangles[i]));
  }
  s << "\n    }\n";
  Layer(s, "Normal", "Normals", mesh.normal, 0);
  Layer(s, "Tangent", "Tangents", mesh.tangent, 0);
  Layer(s, "Color", "Colors", mesh.color, 0);
  for(int i = 0; i < 3; i++)
    Layer(s, "UV", "UV", mesh.uv[i], i);
  for(int i = 0; i < 3; i++)
  {
    s << "    Layer: " << i << " {\n        Version: 100\n";
    auto reference = [&s](const char *type, int index) {
      s << "        LayerElement: {\n            Type: \"LayerElement" << type
        << "\"\n            TypedIndex: " << index << "\n        }\n";
    };
    if(i == 0)
    {
      if(!mesh.normal.values.isEmpty())
        reference("Normal", 0);
      if(!mesh.tangent.values.isEmpty())
        reference("Tangent", 0);
      if(!mesh.color.values.isEmpty())
        reference("Color", 0);
    }
    if(!mesh.uv[i].values.isEmpty())
      reference("UV", i);
    s << "    }\n";
  }
  s << "  }\n  Model: 3, \"Model::Mesh\", \"Mesh\" {\n    Version: 232\n"
    << "    Properties70: {\n"
    << "        P: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\",0,0,0\n"
    << "        P: \"Lcl Rotation\", \"Lcl Rotation\", \"\", \"A\",0,0,0\n"
    << "        P: \"Lcl Scaling\", \"Lcl Scaling\", \"\", \"A\",1,1,1\n"
    << "    }\n    Shading: T\n    Culling: \"CullingOff\"\n  }\n}\n"
    << "Connections: {\n    C: \"OO\",2,3\n    C: \"OO\",3,0\n}\n";
  s.flush();
  if(s.status() != QTextStream::Ok || !file.commit())
  {
    error = file.errorString();
    return false;
  }
  return true;
}
}

#if ENABLE_UNIT_TESTS
#include <QTemporaryDir>
#include "3rdparty/catch/catch.hpp"

TEST_CASE("FBX triangle topology and base vertex", "[fbx]")
{
  QVector<uint32_t> triangles;
  QString error;
  REQUIRE(FBXExporter::Triangulate(Topology::TriangleStrip, {0, 1, 2, 3, UINT32_MAX, 4, 5, 6},
                                   triangles, error));
  CHECK(triangles == QVector<uint32_t>({0, 1, 2, 2, 1, 3, 4, 5, 6}));
  REQUIRE(FBXExporter::Triangulate(Topology::TriangleFan, {0, 1, 2, 3}, triangles, error));
  CHECK(triangles == QVector<uint32_t>({0, 1, 2, 0, 2, 3}));
  CHECK_FALSE(FBXExporter::Triangulate(Topology::LineList, {0, 1}, triangles, error));
  uint32_t vertex = 0;
  bool restart = false;
  REQUIRE(FBXExporter::ResolveIndex(7, -3, UINT32_MAX, vertex, restart, error));
  CHECK(vertex == 4);
  REQUIRE(FBXExporter::ResolveIndex(UINT32_MAX, 5, UINT32_MAX, vertex, restart, error));
  CHECK(restart);
  CHECK_FALSE(FBXExporter::ResolveIndex(1, -3, 0, vertex, restart, error));
  CHECK_FALSE(FBXExporter::ResolveIndex(UINT32_MAX - 1, 5, 0, vertex, restart, error));
}

TEST_CASE("FBX validates mesh and writes an independent-reader fixture", "[fbx]")
{
  QTemporaryDir dir;
  QString error;
  FBXExporter::Mesh mesh;
  QString path = dir.filePath(QStringLiteral("mesh.fbx"));
  CHECK_FALSE(FBXExporter::Write(path, mesh, error));
  mesh.positions = {0, 0, 0, 1, 0, 0, 0, 1, 0};
  mesh.triangles = {0, 1, 2};
  mesh.normal.components = 3;
  mesh.normal.values = {0, 0, 1, 0, 0, 1, 0, 0, 1};
  mesh.tangent.components = 3;
  mesh.tangent.values = {1, 0, 0, 1, 0, 0, 1, 0, 0};
  mesh.color.components = 4;
  mesh.color.values = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
  for(int i = 0; i < 3; i++)
  {
    mesh.uv[i].name = QStringLiteral("UV%1").arg(i);
    mesh.uv[i].components = 2;
    mesh.uv[i].values = {0, 0, 1, 0, 0, 1};
  }
  REQUIRE(FBXExporter::Write(path, mesh, error));
  QByteArray fixture = qgetenv("RENDERDOC_FBX_TEST_OUTPUT");
  if(!fixture.isEmpty())
    REQUIRE(FBXExporter::Write(QString::fromUtf8(fixture), mesh, error));
  mesh.triangles[2] = 3;
  CHECK_FALSE(FBXExporter::Write(path, mesh, error));
  mesh.triangles[2] = 2;
  mesh.normal.values.removeLast();
  CHECK_FALSE(FBXExporter::Write(path, mesh, error));
  mesh.normal.values.clear();
  mesh.positions[0] = std::numeric_limits<double>::infinity();
  CHECK_FALSE(FBXExporter::Write(path, mesh, error));
}
#endif
