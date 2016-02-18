/*
* Waifu2x-opt image restoration filter - VapourSynth plugin
* Copyright (C) 2015  mawen1250
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <fstream>
#include <algorithm>
#include "Waifu2x_Base.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int Waifu2x_Data_Base::arguments_process(const VSMap *in, VSMap *out)
{
    int error;

    // input - clip
    node = vsapi->propGetNode(in, "input", 0, nullptr);
    vi = vsapi->getVideoInfo(node);

    if (!isConstantFormat(vi))
    {
        setError(out, "Invalid input clip, only constant format input supported");
        return 1;
    }
    if ((vi->format->sampleType == stInteger && vi->format->bitsPerSample > 16)
        || (vi->format->sampleType == stFloat && vi->format->bitsPerSample != 32))
    {
        setError(out, "Invalid input clip, only 8-16 bit integer or 32 bit float formats supported");
        return 1;
    }

    // matrix - int
    para.matrix = static_cast<ColorMatrix>(vsapi->propGetInt(in, "matrix", 0, &error));

    if (vi->format->colorFamily == cmYCoCg)
    {
        para.matrix = ColorMatrix::YCgCo;
    }
    else if (error || para.matrix == ColorMatrix::Unspecified)
    {
        para.matrix = para_default.matrix;
    }
    else if (para.matrix != ColorMatrix::GBR && para.matrix != ColorMatrix::bt709
        && para.matrix != ColorMatrix::fcc && para.matrix != ColorMatrix::bt470bg && para.matrix != ColorMatrix::smpte170m
        && para.matrix != ColorMatrix::smpte240m && para.matrix != ColorMatrix::YCgCo && para.matrix != ColorMatrix::bt2020nc
        && para.matrix != ColorMatrix::bt2020c && para.matrix != ColorMatrix::OPP)
    {
        setError(out, "Unsupported \"matrix\" specified");
        return 1;
    }

    // full - bint
    para.full = vsapi->propGetInt(in, "full", 0, &error) != 0;

    if (error)
    {
        if (vi->format->colorFamily == cmGray || vi->format->colorFamily == cmYUV)
        {
            para.full = false;
        }
        else
        {
            para.full = true;
        }
    }

    // block_width - int
    para.block_width = int64ToIntS(vsapi->propGetInt(in, "block_width", 0, &error));

    if (error)
    {
        para.block_width = para_default.block_width;
    }

    // block_height - int
    para.block_height = int64ToIntS(vsapi->propGetInt(in, "block_height", 0, &error));

    if (error)
    {
        para.block_height = para_default.block_height;
    }

    // threads - int
    para.threads = int64ToIntS(vsapi->propGetInt(in, "threads", 0, &error));

    if (error)
    {
        para.threads = para_default.threads;
    }
    else if (para.threads < 0)
    {
        setError(out, "Invalid \"threads\" assigned, must be a non-negative interger");
        return 1;
    }

    /*
    // depth - int
    para.depth = int64ToIntS(vsapi->propGetInt(in, "depth", 0, &error));

    if (error || para.depth == -1)
    {
        para.depth = vi->format->bitsPerSample;
    }

    // sample - int
    para.sample = int64ToIntS(vsapi->propGetInt(in, "sample", 0, &error));

    if (error || para.sample == -1)
    {
        if (vi->format->sampleType == stInteger && para.depth <= 16)
        {
            para.sample = stInteger;
        }
        else
        {
            para.sample = stFloat;
        }
    }

    if (para.sample == stInteger && (para.depth < 8 || para.depth > 16))
    {
        setError(out, "For integer format output, \'depth\' must be an integer in [8, 16]");
        return 1;
    }
    if (para.sample == stFloat && (para.depth != 16 || para.depth != 32))
    {
        setError(out, "For float format output, \'depth\' must be 16 or 32");
        return 1;
    }
    
    // dither - data
    auto dither = vsapi->propGetData(in, "dither", 0, &error);

    if (error)
    {
        para.dither = para_default.dither;
    }
    else
    {
        para.dither = dither;
    }

    if (para.dither != "none" && para.dither != "ordered" && para.dither != "random" && para.dither != "error_diffusion")
    {
        setError(out, "Unrecognized \"dither\" specified, should be \"none\", \"ordered\", \"random\" or \"error_diffusion\"");
        return 1;
    }

    // Initialize z_depth
    init_z_depth(z_depth, para.dither);
    */
    return 0;
}


void Waifu2x_Data_Base::release()
{
    for (auto &x : waifu2x)
    {
        delete x;
        x = nullptr;
    }

    for (auto &x : waifu2x_mutex)
    {
        delete x;
        x = nullptr;
    }

    waifu2x.clear();
    waifu2x_mutex.clear();
}


void Waifu2x_Data_Base::moveFrom(_Myt &right)
{
    waifu2x = std::move(right.waifu2x);
}


void Waifu2x_Data_Base::init_waifu2x(std::vector<Waifu2x *> &context, std::vector<std::mutex *> &mutex,
    int model, int threads, PCType width, PCType height, PCType block_width, PCType block_height,
    VSCore *core, const VSAPI *vsapi)
{
    // Get model file name
    std::string model_name;

    switch (model)
    {
    default:
    case 0:
        model_name = "scale2.0x_model.json";
        break;
    case 1:
        model_name = "noise1_model.json";
        break;
    case 2:
        model_name = "noise2_model.json";
        break;
    }

    // Get model file path
    VSPlugin *plugin = vsapi->getPluginById("Ay.is.baka", core);
    std::string plugin_path(vsapi->getPluginPath(plugin));
    std::string model_path(plugin_path.substr(0, plugin_path.find_last_of('/')) + "/" + model_name);

    // Get model file contents
    std::ifstream ifs(model_path, std::ios::in);
    std::ostringstream oss;
    std::string line;
    while (std::getline(ifs, line))
    {
        oss << line << std::endl;
    }

    // Create a Waifu2x filter object for each thread
    const int vscore_threads = USE_VAPOURSYNTH_MT ? vsapi->getCoreInfo(core)->numThreads : 1;
    context.resize(vscore_threads);
    mutex = std::vector<std::mutex *>(vscore_threads, nullptr);

    for (auto &x : context)
    {
        x = new Waifu2x(oss.str());

        x->set_num_threads(threads);

        const int padding = x->num_steps() * 2;
        block_width = waifu2x_get_optimal_block_size(width, block_width, padding);
        block_height = waifu2x_get_optimal_block_size(height, block_height, padding);
        x->set_image_block_size(block_width, block_height);
    }

    for (auto &x : mutex)
    {
        x = new std::mutex();
    }
}


PCType Waifu2x_Data_Base::waifu2x_get_optimal_block_size(PCType size, PCType block_size, PCType padding)
{
    if (block_size < 0)
    {
        return -block_size;
    }
    else if (block_size == 0)
    {
        return size + padding;
    }
    else
    {
        int optimal_block_size;
        int divisor = 0;

        do
        {
            ++divisor;
            optimal_block_size = (size + divisor - 1) / divisor;
        } while (optimal_block_size > block_size);

        return optimal_block_size + padding;
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void Waifu2x_Process_Base::set_mutex(std::vector<Waifu2x *> &context, std::vector<std::mutex *> &mutex)
{
    release_mutex();

    for (size_t i = 0; i < mutex.size(); ++i)
    {
        if (mutex[i]->try_lock())
        {
            waifu2x = context[i];
            waifu2x_mutex = mutex[i];
            break;
        }
    }

    if (waifu2x == nullptr || waifu2x_mutex == nullptr)
    {
        DEBUG_FAIL("Waifu2x_Process_Base::set_mutex: no mutex is unlocked");
    }
}


void Waifu2x_Process_Base::release_mutex()
{
    if (waifu2x_mutex != nullptr)
    {
        waifu2x_mutex->unlock();
    }

    waifu2x = nullptr;
    waifu2x_mutex = nullptr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
