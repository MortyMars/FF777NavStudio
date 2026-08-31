// STREAM.H
// DÉFINIT LES CLASSES ET MÉTHODES GÉNÉRIQUES DE LECTURE / ÉCRITURE DES ENREGISTREMENTS

#ifndef STREAM_H
#define STREAM_H

#include <string>
#include <stdexcept>
#include <vector>

#include "Types.h"


// DÉCLARATION DE LA CLASSE STREAMREADER ***********************************
class StreamReader
{
    public:
        virtual sint64 getBytes(void *outData, uint64 inSize) =0;
        virtual ~StreamReader() = default;

        int readChar();
        void unreadChar(int ch);
	
    public:
        bool getData(void *outData, uint64 inSize);
        std::string getString(uint64 inSize);
        std::string getWord();

        template<typename Type> bool getValue(uint32 &outDst);
        template<typename Type> bool getValue(uint64 &outDst);
        template<typename Type> bool getValue(sint64 &outDst);
        template<typename Type> bool getValue(sint32 &outDst);
        template<typename Type> bool getValue(float &outDst);
        template<typename Type> bool getValue(double &outDst);
        template<typename Type, typename T> bool getValue(T &outDst);
        template<typename Type, typename T> bool getValueNC(T &outDst);

        std::string getIdent();
        void setReadTextMode(bool inEnabled);
        bool readTextMode();

    private:
        int mLookupChar = -1;
        bool mReadTextMode = false;

    private:
        bool isSeparator(int inCh);
        void skipSpaces();
        template<typename Type, typename T> bool getValueBin(T &outDst);
};
//--------------------------------------------------------------------------


// DÉCLARATION DE LA CLASSE STEAMWRITER ************************************
class StreamWriter
{
    public:
        virtual sint64 putBytes(const void *inData, uint64 inSize) =0;
        virtual ~StreamWriter() = default;
	
    public:
        bool putData(const void *inData, uint64 inSize);
        template<typename Type, typename T> bool putValue(const T &inSrc);
        void putIdent(const std::string &inStr);
        void putNewLine();
        void setWriteTextMode(bool inEnabled);
        bool writeTextMode();

    private:
        bool mWriteTextMode = false;
};
//-------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------
// DÉFINITIONS DES TEMPLATES DES CLASSES STREAMREADER ET STREAMWRITER
// (doivent être définis dans le .h et non dans le .cpp pour éviter des erreurs de compilation)
//-------------------------------------------------------------------------------------------------

    // STREAMREADER TEMPLATES--------------------------------------------------------------------------

    template<typename Type> bool StreamReader::getValue(uint32 &outDst) {
        if (!mReadTextMode)
            return getValueBin<Type>(outDst);
        std::string str = getWord();
        if (!str.length())
            return false;
        outDst = (uint32)std::stoul(str);
        return true;
    }

    template<typename Type> bool StreamReader::getValue(uint64 &outDst) {
        if (!mReadTextMode)
            return getValueBin<Type>(outDst);
        std::string str = getWord();
        if (!str.length())
            return false;
        outDst = (uint64)std::stoull(str);
        return true;
    }

    template<typename Type> bool StreamReader::getValue(sint64 &outDst) {
        if (!mReadTextMode)
            return getValueBin<Type>(outDst);
        std::string str = getWord();
        if (!str.length())
            return false;
        outDst = (sint64)std::stoll(str);
        return true;
    }

    template<typename Type> bool StreamReader::getValue(sint32 &outDst) {
        if (!mReadTextMode)
            return getValueBin<Type>(outDst);
        std::string str = getWord();
        if (!str.length())
            return false;
        outDst = std::stoi(str);
        return true;
    }

    template<typename Type> bool StreamReader::getValue(float &outDst) {
        if (!mReadTextMode)
            return getValueBin<Type>(outDst);
        std::string str = getWord();
        if (!str.length())
            return false;
        outDst = std::stof(str);
        return true;  // CORRIGÉ : était "false"
    }

    template<typename Type> bool StreamReader::getValue(double &outDst) {
        if (!mReadTextMode)
            return getValueBin<double>(outDst);
        std::string str = getWord();
        if (!str.length())
            return false;
        outDst = std::stod(str);
        return true;  // CORRIGÉ : était "false"
    }

    template<typename Type, typename T> bool StreamReader::getValue(T &outDst) {
        if (mReadTextMode) {
            throw std::runtime_error("Attempt to read text file as binary");
        }
        return getValueBin<Type>(outDst);
    }

    template<typename Type, typename T> bool StreamReader::getValueNC(T &outDst) {
        Type value;
        if (!getData(&value, sizeof(Type))) {
            return false;
        }
        outDst = value;
        return true;
    }

    template<typename Type, typename T> bool StreamReader::getValueBin(T &outDst) {
        Type value;
        if (!getData(&value, sizeof(Type))) {
            return false;
        }
        outDst = (T)(value);
        return true;
    }

    // Spécialisation pour std::string
    template<> inline bool StreamReader::getValue<std::string, std::string>(std::string &outDst) {
        uint32 size = 0;
        if (!getData(&size, sizeof(size))) {
            return false;
        }
        std::vector<char> value;
        value.resize(size);
        if (!getData(value.data(), size * sizeof(char))) {
            return false;
        }
        value.push_back(0);
        outDst = value.data();
        return true;
    }



    // STREAMWRITER TEMPLATES -------------------------------------------------------------------------

    template<typename Type, typename T> bool StreamWriter::putValue(const T &inSrc) {
        Type value = (Type)(inSrc);
        if (mWriteTextMode) {
            std::string s = std::to_string(value) + " ";
            return putData(s.c_str(), s.length());
        }
        else {
            return putData(&value, sizeof(Type));
        }
    }

    // Spécialisation pour std::string
    template<> inline bool StreamWriter::putValue<std::string, std::string>(const std::string &inSrc) {
        std::string value = inSrc;
        uint32 size = (uint32)(value.size());
        if (!putData(&size, sizeof(size))) {
            return false;
        }
        return putData(value.data(), size * sizeof(char));
    }

//--------------------------------------------------------------------------

#endif // STREAM_H

