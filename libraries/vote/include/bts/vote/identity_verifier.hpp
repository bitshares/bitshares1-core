#pragma once

#include <bts/vote/types.hpp>

#include <fc/filesystem.hpp>

namespace bts { namespace vote {
namespace detail { class identity_verifier_impl; }

/**
 * @brief The identity_verifier class acts as a database of ID verification requests.
 *
 * Requests which come in are stored as pending verification. A paginated list of pending requests can be retrieved,
 * which returns a list of condensed requests. An individual request can be retrieved by ID, which will mark it as
 * in-processing. The request can then be accepted or rejected, which sets its final status accordingly. At this point,
 * an identity_verification_response will be available for retrieval by a client. All requests for the verification
 * response until this point will receive a null response.
 *
 * This class does *not* do any signing, encryption or decryption. Some other class must encrypt and decrypt messages
 * going over the internet. Furthermore, this class will NOT sign the properties. If a response is returned with an
 * acceptance response, some other class must sign the properties prior to encrypting and returning the response.
 */
class identity_verifier {
   std::shared_ptr<detail::identity_verifier_impl> my;

public:
   identity_verifier();
   ~identity_verifier();

   void open(fc::path data_dir);
   void close();
   bool is_open();

   /**
    * @brief Submit a request to verify the provided identity by filling in the provided fields and signing them.
    * @param request The identity to verify and materials to identify with. Should have property names and salts
    * defined, and the verifier will fill in property values and signatures.
    * @param signature Owner's signature on request.digest()
    * @return Identical semantics to get_verified_identity
    *
    * Submits request for processing by the ID verifier. The request object should contain photographs of the user, and
    * a set of properties that the user wishes to have verified. The names and salts of the properties must be set;
    * other fields may or may not be set. During processing, the verifier fills in the values of the fields and signs
    * them, or rejects the identity and does not sign any properties.
    */
   fc::optional<identity_verification_response> store_new_request(const identity_verification_request& request,
                                                                  const fc::ecc::compact_signature& signature);
   /**
    * @brief Check if an identity verification request has been processed, and if so, get its result
    * @param owner Address of the owner of the identity
    * @return The status and, if accepted, result of the verification request
    *
    * This call may be polled to check for the result of an identity request. If the request has not yet been
    * processed, the result will be a null optional. If the request has been processed and rejected, the result will
    * contain a response with accepted == false and a rejection reason. If the request has been processed and accepted,
    * the result will contain a response with accepted == true and an identity with the values set on its properties.
    * Some other class MUST sign these properties prior to returning the response to a client.
    *
    * Any fields which were not processed by the verifier will have been removed from the accepted identity structure.
    * This means it is safe to sign as verified all fields in the verified_identity returned if the verification was
    * accepted.
    */
   fc::optional<identity_verification_response> get_verified_identity(const address& owner);
};
} } // namespace bts::vote
