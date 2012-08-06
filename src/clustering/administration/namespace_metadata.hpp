#ifndef CLUSTERING_ADMINISTRATION_NAMESPACE_METADATA_HPP_
#define CLUSTERING_ADMINISTRATION_NAMESPACE_METADATA_HPP_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "utils.hpp"
#include <boost/bind.hpp>

#include "clustering/administration/database_metadata.hpp"
#include "clustering/administration/datacenter_metadata.hpp"
#include "clustering/administration/http/json_adapters.hpp"
#include "clustering/administration/persistable_blueprint.hpp"
#include "clustering/reactor/blueprint.hpp"
#include "clustering/reactor/directory_echo.hpp"
#include "clustering/reactor/json_adapters.hpp"
#include "clustering/reactor/metadata.hpp"
#include "containers/uuid.hpp"
#include "http/json/json_adapter.hpp"
#include "rpc/semilattice/joins/deletable.hpp"
#include "rpc/semilattice/joins/macros.hpp"
#include "rpc/semilattice/joins/map.hpp"
#include "rpc/semilattice/joins/vclock.hpp"
#include "rpc/serialize_macros.hpp"

typedef uuid_t namespace_id_t;

/* This is the metadata for a single namespace of a specific protocol. */

/* If you change this data structure, you must also update
`clustering/administration/issues/vector_clock_conflict.hpp`. */

template<class protocol_t>
class namespace_semilattice_metadata_t {
public:
    vclock_t<persistable_blueprint_t<protocol_t> > blueprint;
    vclock_t<datacenter_id_t> primary_datacenter;
    vclock_t<std::map<datacenter_id_t, int> > replica_affinities;
    vclock_t<std::map<datacenter_id_t, int> > ack_expectations;
    vclock_t<std::set<typename protocol_t::region_t> > shards;
    vclock_t<std::string> name;
    vclock_t<int> port;
    vclock_t<region_map_t<protocol_t, machine_id_t> > primary_pinnings;
    vclock_t<region_map_t<protocol_t, std::set<machine_id_t> > > secondary_pinnings;
    vclock_t<std::string> primary_key; //TODO this should actually never be changed...
    vclock_t<database_id_t> database;

    RDB_MAKE_ME_SERIALIZABLE_11(blueprint, primary_datacenter, replica_affinities, ack_expectations, shards, name, port, primary_pinnings, secondary_pinnings, primary_key, database);
};

template<class protocol_t>
RDB_MAKE_SEMILATTICE_JOINABLE_11(namespace_semilattice_metadata_t<protocol_t>, blueprint, primary_datacenter, replica_affinities, ack_expectations, shards, name, port, primary_pinnings, secondary_pinnings, primary_key, database);

template<class protocol_t>
RDB_MAKE_EQUALITY_COMPARABLE_11(namespace_semilattice_metadata_t<protocol_t>, blueprint, primary_datacenter, replica_affinities, ack_expectations, shards, name, port, primary_pinnings, secondary_pinnings, primary_key, database);

//json adapter concept for namespace_semilattice_metadata_t
template <class ctx_t, class protocol_t>
json_adapter_if_t::json_adapter_map_t get_json_subfields(namespace_semilattice_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    json_adapter_if_t::json_adapter_map_t res;
    res["blueprint"] = boost::shared_ptr<json_adapter_if_t>(new json_read_only_adapter_t<vclock_t<persistable_blueprint_t<protocol_t> >, ctx_t>(&target->blueprint, ctx));
    res["primary_uuid"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<datacenter_id_t, ctx_t>(&target->primary_datacenter, ctx));
    res["replica_affinities"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<std::map<datacenter_id_t, int>, ctx_t>(&target->replica_affinities, ctx));
    res["ack_expectations"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<std::map<datacenter_id_t, int>, ctx_t>(&target->ack_expectations, ctx));
    res["name"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<std::string, ctx_t>(&target->name, ctx));
    res["shards"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<std::set<typename protocol_t::region_t>, ctx_t>(&target->shards, ctx));
    res["port"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<int, ctx_t>(&target->port, ctx));
    res["primary_pinnings"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<region_map_t<protocol_t, machine_id_t>, ctx_t>(&target->primary_pinnings, ctx));
    res["secondary_pinnings"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<region_map_t<protocol_t, std::set<machine_id_t> >, ctx_t>(&target->secondary_pinnings, ctx));
    res["primary_key"] = boost::shared_ptr<json_adapter_if_t>(new json_read_only_adapter_t<vclock_t<std::string>, ctx_t>(&target->primary_key, ctx));
    res["database"] = boost::shared_ptr<json_adapter_if_t>(new json_vclock_adapter_t<database_id_t, ctx_t>(&target->database, ctx));
    return res;
}

template <class ctx_t, class protocol_t>
cJSON *render_as_json(namespace_semilattice_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    return render_as_directory(target, ctx);
}

template <class ctx_t, class protocol_t>
void apply_json_to(cJSON *change, namespace_semilattice_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    apply_as_directory(change, target, ctx);
}

template <class ctx_t, class protocol_t>
void on_subfield_change(namespace_semilattice_metadata_t<protocol_t> *, const ctx_t &) { }

/* This is the metadata for all of the namespaces of a specific protocol. */
template <class protocol_t>
class namespaces_semilattice_metadata_t {
public:
    typedef std::map<namespace_id_t, deletable_t<namespace_semilattice_metadata_t<protocol_t> > > namespace_map_t;
    namespace_map_t namespaces;

    /* If a name uniquely identifies a namespace then return it, otherwise
     * return nothing. */
    boost::optional<std::pair<namespace_id_t, deletable_t<namespace_semilattice_metadata_t<protocol_t> > > > get_namespace_by_name(std::string name) {
        boost::optional<std::pair<namespace_id_t, deletable_t<namespace_semilattice_metadata_t<protocol_t> > > >  res;
        for (typename namespace_map_t::iterator it  = namespaces.begin();
                                                it != namespaces.end();
                                                ++it) {
            if (it->second.is_deleted() || it->second.get().name.in_conflict()) {
                return boost::optional<std::pair<namespace_id_t, deletable_t<namespace_semilattice_metadata_t<protocol_t> > > > ();
            }

            if (it->second.get().name.get() == name) {
                if (!res) {
                    res = *it;
                } else {
                    return boost::optional<std::pair<namespace_id_t, deletable_t<namespace_semilattice_metadata_t<protocol_t> > > >();
                }
            }
        }
        return res;
    }

    RDB_MAKE_ME_SERIALIZABLE_1(namespaces);
};

template<class protocol_t>
RDB_MAKE_SEMILATTICE_JOINABLE_1(namespaces_semilattice_metadata_t<protocol_t>, namespaces);

template<class protocol_t>
RDB_MAKE_EQUALITY_COMPARABLE_1(namespaces_semilattice_metadata_t<protocol_t>, namespaces);

// json adapter concept for namespaces_semilattice_metadata_t

template <class ctx_t, class protocol_t>
json_adapter_if_t::json_adapter_map_t get_json_subfields(namespaces_semilattice_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    namespace_semilattice_metadata_t<protocol_t> default_namespace;

    std::set<typename protocol_t::region_t> default_shards;
    default_shards.insert(protocol_t::region_t::universe());
    default_namespace.shards = default_namespace.shards.make_new_version(default_shards, ctx.us);

    /* It's important to initialize this because otherwise it will be
    initialized with a default-constructed UUID, which doesn't initialize its
    contents, so Valgrind will complain. */
    region_map_t<protocol_t, machine_id_t> default_primary_pinnings(protocol_t::region_t::universe(), nil_uuid());
    default_namespace.primary_pinnings = default_namespace.primary_pinnings.make_new_version(default_primary_pinnings, ctx.us);
    default_namespace.database = default_namespace.database.make_new_version(nil_uuid(), ctx.us);

    deletable_t<namespace_semilattice_metadata_t<protocol_t> > default_ns_in_deletable(default_namespace);
    return json_adapter_with_inserter_t<typename namespaces_semilattice_metadata_t<protocol_t>::namespace_map_t, ctx_t>(&target->namespaces, generate_uuid, ctx, default_ns_in_deletable).get_subfields();
}

template <class ctx_t, class protocol_t>
cJSON *render_as_json(namespaces_semilattice_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    return render_as_json(&target->namespaces, ctx);
}

template <class ctx_t, class protocol_t>
void apply_json_to(cJSON *change, namespaces_semilattice_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    apply_as_directory(change, &target->namespaces, ctx);
}

template <class ctx_t, class protocol_t>
void on_subfield_change(namespaces_semilattice_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    on_subfield_change(&target->namespaces, ctx);
}

template <class protocol_t>
class namespaces_directory_metadata_t {
public:
    typedef std::map<namespace_id_t, directory_echo_wrapper_t<reactor_business_card_t<protocol_t> > > reactor_bcards_map_t;

    reactor_bcards_map_t reactor_bcards;

    RDB_MAKE_ME_SERIALIZABLE_1(reactor_bcards);
};

struct namespace_metadata_ctx_t {
    const uuid_t us;
    explicit namespace_metadata_ctx_t(uuid_t _us)
        : us(_us)
    { }
};

inline bool operator==(const namespace_metadata_ctx_t &x, const namespace_metadata_ctx_t &y) {
    return x.us == y.us;
}

// json adapter concept for namespaces_directory_metadata_t

template <class ctx_t, class protocol_t>
json_adapter_if_t::json_adapter_map_t get_json_subfields(namespaces_directory_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    json_adapter_if_t::json_adapter_map_t res;
    res["reactor_bcards"] = boost::shared_ptr<json_adapter_if_t>(new json_read_only_adapter_t<std::map<namespace_id_t, directory_echo_wrapper_t<reactor_business_card_t<protocol_t> > >, ctx_t>(&target->reactor_bcards, ctx));
    return res;
}

template <class ctx_t, class protocol_t>
cJSON *render_as_json(namespaces_directory_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    return render_as_directory(target, ctx);
}

template <class ctx_t, class protocol_t>
void apply_json_to(cJSON *change, namespaces_directory_metadata_t<protocol_t> *target, const ctx_t &ctx) {
    apply_as_directory(change, target, ctx);
}

template <class ctx_t, class protocol_t>
void on_subfield_change(UNUSED namespaces_directory_metadata_t<protocol_t> *target, UNUSED const ctx_t &ctx) { }

#endif /* CLUSTERING_ADMINISTRATION_NAMESPACE_METADATA_HPP_ */
