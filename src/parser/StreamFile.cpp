// STREAMFILE.CPP DÉFINIT LES MÉTHODES DE LECTURE / ÉCRITURE DES ENREGISTREMENT
// AU FORMAT BINAIRE

#include <vector>

#include "xxhash.h"     // Pour accès aux fonctions de hashage 64 bits
#include "StreamFile.h"


//**************************************************************************
bool StreamFile::create(const std::string &inPath, bool inCreate, bool inReset) {
    if (inReset)remove(inPath.data());

    mFile = fopen(inPath.data(), "r+b");

    if (!mFile) {
        if (!inCreate)return false;



        mFile = fopen(inPath.data(), "w+b");

        if (!mFile)return false;
    }

    fseek(mFile, 0, SEEK_SET);

    return true;
}
//--------------------------------------------------------------------------

StreamFile::StreamFile() {
    mFile = nullptr;
}
//--------------------------------------------------------------------------

StreamFile::~StreamFile() {
    if (!mFile)return;
    fclose(mFile);
}
//--------------------------------------------------------------------------

sint64 StreamFile::getBytes(void *inData, uint64 inSize) {
    if (!inData) {
        fseek(mFile, (long) (inSize), SEEK_CUR);
        return (sint64) (inSize);
    }
    return (sint64) (fread(inData, 1, inSize, mFile));
}
//--------------------------------------------------------------------------

sint64 StreamFile::putBytes(const void *inData, uint64 inSize) {
    if (!inData) {
        fseek(mFile, (long) (inSize), SEEK_CUR);
        return (sint64) (inSize);
    }
    return (sint64) (fwrite(inData, 1, inSize, mFile));
}
//--------------------------------------------------------------------------

uint64 StreamFile::size() {
    long p = ftell(mFile);
    fseek(mFile, 0, SEEK_END);
    long s = ftell(mFile);
    fseek(mFile, p, SEEK_SET);
    return (uint64)s;
}
//--------------------------------------------------------------------------

uint64 StreamFile::hash() {
    uint64 size = this->size();
    long p = ftell(mFile);
    std::vector<char> data(size);
    fseek(mFile, 0, SEEK_SET);
    if (fread(&data[0], 1, size, mFile) != size) {
        return 0;
    }
    // uint64 hash = ndbl::hash64(&data[0], size);
    uint64 hash = XXHash64::hash(&data[0], size, 0); // cf. construction de la fonction
                                                     // dans l'entête xxhash.h
    fseek(mFile, p, SEEK_SET);
    return hash;
}
//--------------------------------------------------------------------------

//**************************************************************************

