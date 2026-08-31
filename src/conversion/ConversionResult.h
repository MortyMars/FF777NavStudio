#pragma once

// ============================================================================
// ConversionResult.h
// Résultat d'une conversion UserXxx -> Xxx (modèle cible). Une conversion
// peut échouer proprement (ident non résolvable, valeur de liste de choix
// inconnue) sans jamais planter — c'est le pendant, côté saisie, de la
// tolérance aux lignes malformées déjà en place côté NavDataReader.
// ============================================================================

#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

namespace navstud::conversion {

template <typename T>
class ConversionResult
{
public:
    static ConversionResult ok(T value)
    {
        ConversionResult r;
        r.mValue = std::move(value);
        return r;
    }

    static ConversionResult failed(QString error)
    {
        ConversionResult r;
        r.mErrors << std::move(error);
        return r;
    }

    static ConversionResult failed(QStringList errors)
    {
        ConversionResult r;
        r.mErrors = std::move(errors);
        return r;
    }

    bool isOk() const { return mValue.has_value(); }
    explicit operator bool() const { return isOk(); }

    const T& value() const { return *mValue; }
    T&       value()       { return *mValue; }

    const QStringList& errors() const { return mErrors; }

private:
    std::optional<T> mValue;
    QStringList       mErrors;
};

} // namespace navstud::conversion
