#include "videodecoding.h"

#include "decodingchoice.h"

bool VideoDecoding::softwareOnly() const
{
    return decodingchoice::session().softwareOnly;
}

QString VideoDecoding::inWords() const
{
    return decodingchoice::inWords(decodingchoice::session());
}

bool VideoDecoding::guarded() const
{
    return decodingchoice::session().guarded;
}

QString VideoDecoding::guardReason() const
{
    return decodingchoice::session().guardReason;
}

bool VideoDecoding::guardInForce() const
{
    return decodingchoice::session().by == decodingchoice::By::Guard;
}

bool VideoDecoding::overridden() const
{
    return decodingchoice::session().overridden;
}
