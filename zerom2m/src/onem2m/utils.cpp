
#include <zerom2m/onem2m/types/enums.h>
#include <zerom2m/onem2m/types/primitives.h>
#include <zerom2m/onem2m/types/resources.h>
#include <zerom2m/onem2m/utils.h>

#include <circle/string.h>

namespace zerom2m::onem2m
{

using namespace zerom2m::onem2m::types;

const ResourceBase *GetResourceBase(const PrimitiveContent &pc)
{
    const ResourceBase *r;
    if ((r = pc.GetIf<Container>()) != nullptr) return r;
    if ((r = pc.GetIf<ContentInstance>()) != nullptr) return r;
    if ((r = pc.GetIf<AE>()) != nullptr) return r;
    if ((r = pc.GetIf<Subscription>()) != nullptr) return r;
    if ((r = pc.GetIf<CSEBase>()) != nullptr) return r;
    return nullptr;
}

CString NormalizePath(const CString &path)
{
    if (path.GetLength() > 0 && path.c_str()[0] == '/') return CString(path.c_str() + 1);
    return path;
}

bool isValidResourceName(CString rn)
{
    // Validate resourceName characters: allow alnum and -._~ only
    if (rn.GetLength() != 0) {
        for (size_t i = 0; i < rn.GetLength(); ++i) {
            char c = rn.c_str()[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '.' || c == '_' || c == '~')) {
                return false;
            }
        }
        return true;
    }
    return false;
}

// Helper: check whether a request primitive is minimally valid
bool isValidRequest(const RequestPrimitive &req, CString &errMsg)
{
    if (req.to.GetLength() == 0) {
        errMsg = "Missing mandatory parameter: to";
        return false;
    }
    // The 'from' (originator) is mandatory for most operations, but some
    // bindings/operations (notably Create during AE self-registration) may
    // omit it. Allow empty originator for Create requests.
    if (req.from.GetLength() == 0) {
        if (req.op != Operation::Create) {
            errMsg = "Missing mandatory parameter: from (fr)";
            return false;
        }
    }
    if (req.requestIdentifier.GetLength() == 0) {
        errMsg = "Missing mandatory parameter: requestIdentifier (rqi)";
        return false;
    }
    if (req.op == Operation::Create && !req.resourceType.has_value()) {
        errMsg = "Create request missing resourceType (ty)";
        return false;
    }
    return true;
}

ResponsePrimitive
makeResponse(const RequestPrimitive &req, ResponseStatusCode rsc, PrimitiveContent content)
{
    ResponsePrimitive rsp;
    rsp.responseStatusCode = rsc;
    rsp.to                 = req.from;
    rsp.from               = req.to;
    rsp.requestIdentifier  = req.requestIdentifier;
    if (content.kind() != PrimitiveContentKind::None) rsp.content = content;
    if (req.releaseVersionIndicator) rsp.releaseVersionIndicator = req.releaseVersionIndicator;
    return rsp;
}

} // namespace zerom2m::onem2m