// STREAMFILE.H
// DÉCLARE LES CLASSES DÉRIVÉES DE STREAM.H

#ifndef STREAMFILE_H
#define STREAMFILE_H

#include <memory>

#include "Stream.h"


typedef std::shared_ptr<class StreamFile> StreamFilePtr;


//***************************************************************************************
// Déclaration de le Classe StreamFile dérivée de StreamReader et StreamWriter
class StreamFile : public StreamReader, public StreamWriter {

    private:
        FILE * mFile;

    public:
        bool create(const std::string & inPath, bool inCreate = false, bool inReset = false);
        StreamFile();
        ~StreamFile() override;

    //public:
        sint64 getBytes(void * inData, uint64 inSize) override;
        sint64 putBytes(const void * inData, uint64 inSize) override;

    //public:
        uint64 size();
        uint64 hash();

    //public:
        static StreamFilePtr initialize(const std::string & inPath, bool inCreate = false, bool inReset = false) {
            auto * s = new StreamFile();
            if (s->create(inPath, inCreate, inReset))
                return std::shared_ptr<StreamFile>(s);
            delete s;
            return nullptr;
        }
};
// Fin de déclaration de la Classe ******************************************************


#endif // STREAM_FILE_H

