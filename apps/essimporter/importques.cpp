#include "importques.hpp"

#include <components/esm3/esmreader.hpp>

namespace ESSImport
{

    void QUES::load(ESM::ESMReader &esm)
    {
        while (esm.isNextSub("DATA"))
            mInfo.push_back(esm.getHString());
    }

}
