#include "loadweap.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"
#include "components/esm/defs.hpp"

namespace ESM
{
    void Weapon::load(ESMReader &esm, bool &isDeleted)
    {
        isDeleted = false;
        mRecordFlags = esm.getRecordFlags();

        bool hasName = false;
        bool hasData = false;
        while (esm.hasMoreSubs())
        {
            esm.getSubName();
            switch (esm.retSubName().toInt())
            {
                case SREC_NAME:
                    mId = esm.getHString();
                    hasName = true;
                    break;
                case fourCC("MODL"):
                    mModel = esm.getHString();
                    break;
                case fourCC("FNAM"):
                    mName = esm.getHString();
                    break;
                case fourCC("WPDT"):
                    esm.getHTSized<32>(mData);
                    hasData = true;
                    break;
                case fourCC("SCRI"):
                    mScript = esm.getHString();
                    break;
                case fourCC("ITEX"):
                    mIcon = esm.getHString();
                    break;
                case fourCC("ENAM"):
                    mEnchant = esm.getHString();
                    break;
                case SREC_DELE:
                    esm.skipHSub();
                    isDeleted = true;
                    break;
                default:
                    esm.fail("Unknown subrecord");
            }
        }

        if (!hasName)
            esm.fail("Missing NAME subrecord");
        if (!hasData && !isDeleted)
            esm.fail("Missing WPDT subrecord");
    }
    void Weapon::save(ESMWriter &esm, bool isDeleted) const
    {
        esm.writeHNCString("NAME", mId);

        if (isDeleted)
        {
            esm.writeHNString("DELE", "", 3);
            return;
        }

        esm.writeHNCString("MODL", mModel);
        esm.writeHNOCString("FNAM", mName);
        esm.writeHNT("WPDT", mData, 32);
        esm.writeHNOCString("SCRI", mScript);
        esm.writeHNOCString("ITEX", mIcon);
        esm.writeHNOCString("ENAM", mEnchant);
    }

    void Weapon::blank()
    {
        mRecordFlags = 0;
        mData.mWeight = 0;
        mData.mValue = 0;
        mData.mType = 0;
        mData.mHealth = 0;
        mData.mSpeed = 0;
        mData.mReach = 0;
        mData.mEnchant = 0;
        mData.mChop[0] = mData.mChop[1] = 0;
        mData.mSlash[0] = mData.mSlash[1] = 0;
        mData.mThrust[0] = mData.mThrust[1] = 0;
        mData.mFlags = 0;

        mName.clear();
        mModel.clear();
        mIcon.clear();
        mEnchant.clear();
        mScript.clear();
    }
}
