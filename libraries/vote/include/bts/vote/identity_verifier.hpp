#pragma once

#include <bts/vote/types.hpp>

#include <fc/filesystem.hpp>

namespace bts { namespace vote {
namespace detail { class identity_verifier_impl; }

/**
 * @brief The identity_verifier class acts as a database of ID verification requests.
 *
 * Requests which come in are stored as pending verification. When requests arrive, they are assigned a unique
 * timestamp in microseconds, which is subsequently used as the request ID. A paginated list of pending requests can be
 * retrieved, which returns a list of condensed requests. An individual request can be retrieved by ID, which will mark
 * it as in-processing. The request can then be accepted or rejected, which sets its final status accordingly. At this
 * point, an identity_verification_response will be available for retrieval by a client. All requests for the
 * verification response until this point will receive a null response.
 *
 * This class does *not* do any signing, encryption or decryption. Some other class must encrypt and decrypt messages
 * going over the internet. Furthermore, this class will NOT sign the properties. Some other class must sign the
 * properties prior to encrypting and returning the response. When a request is resolved, only the properties the
 * verifier approved of will be retained; all other properties will be removed from the local database. This means that
 * the class doing the signing can safely sign all properties in the final response.
 */
class identity_verifier {
   std::shared_ptr<detail::identity_verifier_impl> my;

public:
   identity_verifier();
   ~identity_verifier();

   void open(fc::path data_dir);
   void close();
   bool is_open() const;

   /**
    * @brief Submit a request to verify the provided identity by filling in the provided fields and signing them.
    * @param request The identity to verify and materials to identify with. Should have property names and salts
    * defined, and the verifier will fill in property values and signatures.
    * @param owner_key Owner's public key
    * @return Identical semantics to get_verified_identity
    *
    * Submits request for processing by the ID verifier. The request object should contain photographs of the user, and
    * a set of properties that the user wishes to have verified. The names and salts of the properties must be set;
    * other fields may or may not be set. During processing, the verifier fills in the values of the fields and signs
    * them, or rejects the identity and does not sign any properties.
    */
   fc::optional<identity_verification_response> store_new_request(const identity_verification_request& request,
                                                                  const blockchain::public_key_type& owner_key);
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
    *
    * @throws fc::exception If owner has never requested an identity verification
    */
   fc::optional<identity_verification_response> get_verified_identity(const address& owner) const;

   /**
    * @brief Retrieve a set of requests of a certain status.
    * @param status Only requests with this status will be returned
    * @param after_time Only requests received after this time will be returned
    * @param limit The maximum number of records to return
    * @return Up to limit requests received after after_time
    *
    * This function is used to list the verification requests with a given status. This method can be called any number
    * of times and will not alter the state of any requests. The after_time and limit parameters allow pagination of
    * requests, so the first ten can be requested by calling with no parameters, the next ten can be listed by passing
    * to after_time the of timestamp the newest record from the first call, etc.
    */
   vector<identity_verification_request_summary> list_requests(request_status_enum status = awaiting_processing,
                                                               fc::microseconds after_time = fc::microseconds(),
                                                               uint32_t limit = 10) const;
   /**
    * @brief Retrieve full request record for a request, but don't mark it as in-processing yet.
    * @param request_id Timestamp of the request to retrieve. This uniquely identifies the request.
    * @return The request to peek at
    *
    * This function can be used to examine a request without actually processing it. Do not call this function to begin
    * processing a new request! Use take_next_request() for that. Do not process requests returned by this function.
    *
    * @throws fc::key_not_found_exception If request_id does not match any known request
    */
   identity_verification_request peek_request(fc::microseconds request_id) const;

   /**
    * @brief Retrieves a particular pending request, and marks it as in-processing.
    * @param request_id Timestamp of the request to retrieve. This uniquely identifies the request.
    * @return The request to process.
    *
    * This function allows the caller to claim a particular request to work on. The request must be in the
    * awaiting_processing state, and it will be changed to in_processing.
    *
    * @throws fc::key_not_found_exception If request_id does not match any known request
    * @throws fc::exception If no awaiting_processing request has the specified ID
    */
   identity_verification_request take_pending_request(fc::microseconds request_id);
   /**
    * @brief Retrieve the next pending request in line
    * @return The next pending request, or null if no pending requests exist
    *
    * This function gets the next pending request and marks it as in-processing, so it will no longer appear in
    * list_pending_requests. This is the proper function to call to get a request to begin processing, as it guarantees
    * that no two calls will return the same request.
    */
   fc::optional<identity_verification_request> take_next_request();
   /**
    * @brief Mark request as resolved and set its response.
    * @param request_id Timestamp of the request to resolve. This uniquely identifies the request.
    * @param response Response to the request
    * @param skip_soft_checks If true, soft uniqueness checks will be skipped.
    *
    * This will mark the request as resolved, store the decision, and allow a client to retrieve the resolution. Note
    * that ALL properties present in the response will be regarded as verified and will be signed. This means that it
    * is the responsibility of the caller to ensure that no properties are passed in response.verified_identity which
    * are not approved of.
    *
    * If the response is marked as being accepted, it will still be required to pass a series of soft and hard
    * uniqueness checks before it will be returned to the user. If a soft uniqueness check fails, the record will be
    * marked as needs_further_review and its list of conflicting_ids will be populated with the conflicting records.
    * If a hard uniqueness check fails, the record will automatically be rejected as a duplicate identity.
    *
    * @throws fc::key_not_found_exception If request_id does not match any known request
    * @throws fc::exception If response.accepted == true, but response.verified_identity == null
    * @throws fc::exception If response.accepted == true, but response.expiration_date == null
    * @throws fc::exception If response.verified_identity contains a property that was not in the request
    */
   void resolve_request(fc::microseconds request_id, const identity_verification_response& response,
                        bool skip_soft_checks);

   /**
    * @brief Get all data known about a particular record.
    * @param record_id The record ID to get information on
    * @return Object containing all known information on the specified record.
    */
   fc::variant fetch_record(fc::microseconds record_id) const;
};

typedef std::shared_ptr<identity_verifier> identity_verifier_ptr;
} } // namespace bts::vote
