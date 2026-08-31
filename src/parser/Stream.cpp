// STREAM.CPP DÉFINIT LES MÉTHODES NON TEMPLATES DE LA CLASSE STREAMREADER

//*************************************************************************************************
//
//             DÉFINITION DES MÉTHODES **NON-TEMPLATES** DE LA CLASSE STREAMREADER
//     Les Templates devant être déclarées dans le .h pour éviter des erreurs de compilation
//     on ne retrouve ici -dans le .cpp- que les définitions des méthodes non-templates
//
//*************************************************************************************************

#include "Stream.h"

    int StreamReader::readChar() {
        if (mLookupChar != -1) {
            int res = mLookupChar;
            mLookupChar = -1;
            return res;
        }
        char buf;
        if (!getBytes(&buf, 1)) {
            return -1;
        }
        else {
            return buf;
        }
    }

    void StreamReader::unreadChar(int ch) {
        mLookupChar = ch;
    }

    bool StreamReader::getData(void *outData, uint64 inSize) {
        return getBytes(outData, inSize) == (sint64)(inSize);
    }

    std::string StreamReader::getString(uint64 inSize) {
        std::vector<char> buf;
        buf.resize(inSize + 1);
        if (!getData(&buf[0], inSize)) {
            return "";
        }
        buf[inSize] = 0;
        return std::string(&buf[0]);
    }

    std::string StreamReader::getWord() {
        std::string res;
        skipSpaces();
        int ch = readChar();
        while ((-1 != ch) && (!isSeparator((char)ch))) {
            res += (char)ch;
            ch = readChar();
        }
        return res;
    }

    std::string StreamReader::getIdent() {
        if (!mReadTextMode) {
            uint64 size = 0;                // CORRECTION : initialisation à 0 pour éviter un avertissement
            if (!getValue<uint64>(size)) {  // CORRECTION : vérification du retour
                return "";                  // Retourner une chaîne vide en cas d'échec
            }
            std::string ident;
            ident.resize(size);
            if (!getData((void *)(ident.data()), ident.size())) {  // AMÉLIORATION : vérification
                return "";                  // Retourner une chaîne vide en cas d'échec de lecture
            }
            return ident;
        }
        else {
            return getWord();
        }
    }

    void StreamReader::setReadTextMode(bool inEnabled) {
        mReadTextMode = inEnabled;
    }

    bool StreamReader::readTextMode() {
        return mReadTextMode;
    }

    bool StreamReader::isSeparator(int inCh) {
        return (' ' == inCh) || ('\t' == inCh) || ('\r' == inCh) || ('\n' == inCh);
    }

    void StreamReader::skipSpaces() {
        int ch = readChar();
        while (isSeparator(ch) && (-1 != (int)ch)) {
            ch = readChar();
        }
        unreadChar(ch);
    }
//-------------------------------------------------------------------------------------------------



//*************************************************************************************************
//
//             DÉFINITION DES MÉTHODES **NON-TEMPLATES** DE LA CLASSE STREAMWRITER
//     Les Templates devant être déclarées dans le .h pour éviter des erreurs de compilation
//     on ne retrouve ici -dans le .cpp- que les définitions des méthodes non-templates
//
//*************************************************************************************************

    bool StreamWriter::putData(const void *inData, uint64 inSize) {
        return putBytes(inData, inSize) == (sint64)(inSize);
    }

    void StreamWriter::putIdent(const std::string &inStr) {
        if (!mWriteTextMode) {
            putValue<uint32>(inStr.length());
            putData(inStr.c_str(), inStr.length());  // AJOUTÉ : manquait l'écriture des données
        }
        else {
            if (inStr.length()) {
                putData(inStr.c_str(), inStr.length());
            }
            else {
                putData("\"\"", 2);
            }
            putData(" ", 1);
        }
    }

    void StreamWriter::putNewLine() {
        if (mWriteTextMode) {
            putData("\n", 1);
        }
    }

    void StreamWriter::setWriteTextMode(bool inEnabled) {
        mWriteTextMode = inEnabled;
    }

    bool StreamWriter::writeTextMode() {
        return mWriteTextMode;
    }

//-------------------------------------------------------------------------------------------------



