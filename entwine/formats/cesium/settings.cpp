/******************************************************************************
* Copyright (c) 2016, Connor Manning (connor@hobu.co)
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

#include <entwine/formats/cesium/settings.hpp>

namespace entwine
{
namespace cesium
{

Settings::Settings(
        std::size_t tilesetSplit,
        double geometricErrorDivisor,
        std::string coloring,
        bool truncate)
    : m_tilesetSplit(tilesetSplit)
    , m_geometricErrorDivisor(geometricErrorDivisor)
    , m_coloring(coloring)
    , m_truncate(truncate)
{
    if (!m_tilesetSplit) m_tilesetSplit = 8;
    if (m_geometricErrorDivisor == 0.0) m_geometricErrorDivisor = 8.0;
}

Settings::Settings(const Json::Value& json)
    : Settings(
            json["tilesetSplit"].asUInt64(),
            json["geometricErrorDivisor"].asDouble(),
            json["coloring"].asString(),
            json["truncate"].asBool())
{ }

Json::Value Settings::toJson() const
{
    Json::Value json;
    json["tilesetSplit"] = Json::UInt64(m_tilesetSplit);
    json["geometricErrorDivisor"] = m_geometricErrorDivisor;
    if (m_coloring.size()) json["coloring"] = m_coloring;
    if (m_truncate) json["truncate"] = true;
    return json;
}

} // namespace cesium
} // namespace entwine

