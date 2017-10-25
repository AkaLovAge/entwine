/******************************************************************************
* Copyright (c) 2016, Connor Manning (connor@hobu.co)
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include <json/json.h>

namespace entwine
{

namespace arbiter { class Arbiter; }

class Bounds;
class Builder;
class Config;
class Delta;
class Manifest;
class Subset;

class ConfigParser
{
public:
    static Json::Value defaults();

    static std::unique_ptr<Builder> getBuilder(
            Json::Value json,
            std::shared_ptr<arbiter::Arbiter> arbiter = nullptr);

    static std::string directorify(std::string path);

private:
    static void normalizeInput(
            Config& config,
            const arbiter::Arbiter& arbiter);

    static void infer(Config& config);

    static std::unique_ptr<Subset> maybeAccommodateSubset(
            Json::Value& json,
            const Bounds& boundsConforming,
            const Delta* delta);
};

} // namespace entwine

