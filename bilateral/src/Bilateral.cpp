#include "Bilateral.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void VS_CC BilateralInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi)
{
    BilateralData *d = reinterpret_cast<BilateralData *>(*instanceData);

    vsapi->setVideoInfo(d->vi, 1, node);
}

const VSFrameRef *VS_CC BilateralGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi)
{
    const BilateralData *d = reinterpret_cast<BilateralData *>(*instanceData);

    if (activationReason == arInitial)
    {
        vsapi->requestFrameFilter(n, d->node, frameCtx);
        if(d->joint) vsapi->requestFrameFilter(n, d->rnode, frameCtx);
    }
    else if (activationReason == arAllFramesReady)
    {
        const VSFrameRef *src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSFormat *fi = vsapi->getFrameFormat(src);
        int width = vsapi->getFrameWidth(src, 0);
        int height = vsapi->getFrameHeight(src, 0);
        const int planes[] = { 0, 1, 2 };
        const VSFrameRef * cp_planes[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };
        VSFrameRef *dst = vsapi->newVideoFrame2(fi, width, height, cp_planes, planes, src, core);

        const VSFrameRef *ref = d->joint ? vsapi->getFrameFilter(n, d->rnode, frameCtx) : src;
        
        if (d->vi->format->bytesPerSample == 1)
        {
            Bilateral2D<uint8_t>(dst, src, ref, d, vsapi);
        }
        else if (d->vi->format->bytesPerSample == 2)
        {
            Bilateral2D<uint16_t>(dst, src, ref, d, vsapi);
        }

        vsapi->freeFrame(src);
        if (d->joint) vsapi->freeFrame(ref);

        return dst;
    }

    return nullptr;
}

void VS_CC BilateralFree(void *instanceData, VSCore *core, const VSAPI *vsapi)
{
    BilateralData *d = reinterpret_cast<BilateralData *>(instanceData);

    delete d;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void VS_CC BilateralCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi)
{
    BilateralData *data = new BilateralData(vsapi);
    BilateralData &d = *data;

    int error;
    int i, n, m, o;

    d.node = vsapi->propGetNode(in, "input", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.node);

    if (!d.vi->format)
    {
        delete data;
        vsapi->setError(out, "bilateral.Bilateral: Invalid input clip, Only constant format input supported");
        return;
    }
    if (d.vi->format->sampleType != stInteger || (d.vi->format->bytesPerSample != 1 && d.vi->format->bytesPerSample != 2))
    {
        delete data;
        vsapi->setError(out, "bilateral.Bilateral: Invalid input clip, Only 8-16 bit int formats supported");
        return;
    }

    d.rnode = vsapi->propGetNode(in, "ref", 0, &error);
    if (error)
    {
        d.joint = false;
    }
    else
    {
        d.rvi = vsapi->getVideoInfo(d.rnode);
        d.joint = true;

        if (!d.rvi->format)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: Invalid clip \"ref\", Only constant format input supported");
            return;
        }
        if (d.rvi->format->sampleType != stInteger || (d.rvi->format->bytesPerSample != 1 && d.rvi->format->bytesPerSample != 2))
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: Invalid clip \"ref\", Only 8-16 bit int formats supported");
            return;
        }
        if (d.vi->width != d.rvi->width || d.vi->height != d.rvi->height)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: input clip and clip \"ref\" must be of the same size");
            return;
        }
        if (d.vi->format->colorFamily != d.rvi->format->colorFamily)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: input clip and clip \"ref\" must be of the same color family");
            return;
        }
        if (d.vi->format->subSamplingH != d.rvi->format->subSamplingH || d.vi->format->subSamplingW != d.rvi->format->subSamplingW)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: input clip and clip \"ref\" must be of the same subsampling");
            return;
        }
        if (d.vi->format->bitsPerSample != d.rvi->format->bitsPerSample)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: input clip and clip \"ref\" must be of the same bit depth");
            return;
        }
    }

    m = vsapi->propNumElements(in, "sigmaS");
    for (i = 0; i < 3; i++)
    {
        if (i < m)
        {
            d.sigmaS[i] = vsapi->propGetFloat(in, "sigmaS", i, nullptr);
        }
        else if (i == 0)
        {
            d.sigmaS[0] = 3.0;
        }
        else if (i == 1 && d.isYUV() && d.vi->format->subSamplingH && d.vi->format->subSamplingW) // Reduce sigmaS for sub-sampled chroma planes by default
        {
            d.sigmaS[1] = d.sigmaS[0] / std::sqrt((1 << d.vi->format->subSamplingH)*(1 << d.vi->format->subSamplingW));
        }
        else
        {
            d.sigmaS[i] = d.sigmaS[i - 1];
        }

        if (d.sigmaS[i] < 0)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: Invalid \"sigmaS\" assigned, must be non-negative float number");
            return;
        }
    }

    m = vsapi->propNumElements(in, "sigmaR");
    for (i = 0; i < 3; i++)
    {
        if (i < m)
        {
            d.sigmaR[i] = vsapi->propGetFloat(in, "sigmaR", i, nullptr);
        }
        else if (i == 0)
        {
            d.sigmaR[i] = 0.02;
        }
        else
        {
            d.sigmaR[i] = d.sigmaR[i - 1];
        }

        if (d.sigmaR[i] < 0)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: Invalid \"sigmaR\" assigned, must be non-negative float number");
            return;
        }
    }

    n = d.vi->format->numPlanes;
    m = vsapi->propNumElements(in, "planes");
    for (i = 0; i < 3; i++)
    {
        if (i > 0 && d.isYUV()) // Chroma planes are not processed by default
            d.process[i] = 0;
        else
            d.process[i] = m <= 0;
    }
    for (i = 0; i < m; i++) {
        o = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));
        if (o < 0 || o >= n)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: plane index out of range");
            return;
        }
        if (d.process[o])
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: plane specified twice");
            return;
        }
        d.process[o] = 1;
    }
    for (i = 0; i < 3; i++)
    {
        if (d.sigmaS[i] == 0 || d.sigmaR[i] == 0)
            d.process[i] = 0;
    }

    m = vsapi->propNumElements(in, "algorithm");
    for (i = 0; i < 3; i++)
    {
        if (i < m)
        {
            d.algorithm[i] = int64ToIntS(vsapi->propGetInt(in, "algorithm", i, nullptr));
        }
        else if (i == 0)
        {
            d.algorithm[i] = 0;
        }
        else
        {
            d.algorithm[i] = d.algorithm[i - 1];
        }

        if (d.algorithm[i] < 0 || d.algorithm[i] > 2)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: Invalid \"algorithm\" assigned, must be integer ranges in [0,2]");
            return;
        }
    }

    m = vsapi->propNumElements(in, "PBFICnum");
    for (i = 0; i < 3; i++)
    {
        if (i < m)
        {
            d.PBFICnum[i] = int64ToIntS(vsapi->propGetInt(in, "PBFICnum", i, nullptr));
        }
        else if (i == 0)
        {
            d.PBFICnum[i] = 0;
        }
        else
        {
            d.PBFICnum[i] = d.PBFICnum[i - 1];
        }

        if (d.PBFICnum[i] < 0 || d.PBFICnum[i] == 1 || d.PBFICnum[i] > 256)
        {
            delete data;
            vsapi->setError(out, "bilateral.Bilateral: Invalid \"PBFICnum\" assigned, must be integer ranges in [0,256] except 1");
            return;
        }
    }

    // Set parameters for each algorithm and select appropriate algorithm for each plane if algorithm[plane] == 0
    d.Bilateral2D_1_Paras();
    d.Bilateral2D_2_Paras();
    d.algorithm_select();

    // Initialize Gaussian function spatial/range weight LUT
    d.GS_LUT_Init();
    d.GR_LUT_Init();

    // Create filter
    vsapi->createFilter(in, out, "Bilateral", BilateralInit, BilateralGetFrame, BilateralFree, fmParallel, 0, data, core);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
