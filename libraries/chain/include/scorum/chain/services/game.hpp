#pragma once
#include <scorum/chain/schema/game_object.hpp>
#include <scorum/chain/services/service_base.hpp>
#include <scorum/protocol/betting/game.hpp>
#include <scorum/protocol/betting/market.hpp>
#include <scorum/protocol/betting/wincase.hpp>
#include <scorum/chain/dba/db_accessor_fwd.hpp>

namespace scorum {
namespace chain {

struct dynamic_global_property_service_i;
struct betting_property_service_i;

struct game_service_i : public base_service_i<game_object>
{
    using base_service_i<game_object>::is_exists;

    virtual const game_object& create_game(const uuid_type& uuid,
                                           const account_name_type& moderator,
                                           const std::string& json_metadata,
                                           fc::time_point_sec start,
                                           uint32_t auto_resolve_delay_sec,
                                           const game_type& game,
                                           const fc::flat_set<market_type>& markets)
        = 0;

    virtual void finish(const game_object& game, const fc::flat_set<wincase_type>& wincases) = 0;
    virtual void update_markets(const game_object& game, const fc::flat_set<market_type>& markets) = 0;

    virtual bool is_exists(int64_t game_id) const = 0;
    virtual bool is_exists(const uuid_type& uuid) const = 0;

    virtual const game_object& get_game(int64_t game_id) const = 0;
    virtual const game_object& get_game(const uuid_type& uuid) const = 0;

    virtual view_type get_games() const = 0;
    virtual std::vector<object_cref_type> get_games(fc::time_point_sec start) const = 0;

    virtual std::vector<object_cref_type> get_games_to_resolve(fc::time_point_sec resolve_time) const = 0;
    virtual std::vector<object_cref_type> get_games_to_auto_resolve(fc::time_point_sec resolve_time) const = 0;
};

class dbs_game : public dbs_service_base<game_service_i>
{
    friend class dbservice_dbs_factory;

protected:
    explicit dbs_game(database& db);

public:
    using base_service_i<game_object>::is_exists;

    virtual const game_object& create_game(const uuid_type& uuid,
                                           const account_name_type& moderator,
                                           const std::string& json_metadata,
                                           fc::time_point_sec start,
                                           uint32_t auto_resolve_delay_sec,
                                           const game_type& game,
                                           const fc::flat_set<market_type>& markets) override;
    virtual void finish(const game_object& game, const fc::flat_set<wincase_type>& wincases) override;
    virtual void update_markets(const game_object& game, const fc::flat_set<market_type>& markets) override;

    virtual bool is_exists(int64_t game_id) const override;
    bool is_exists(const uuid_type& uuid) const override;

    virtual const game_object& get_game(int64_t game_id) const override;
    virtual const game_object& get_game(const uuid_type& uuid) const override;
    virtual std::vector<object_cref_type> get_games(fc::time_point_sec start) const override;

    virtual view_type get_games() const override;

    std::vector<dbs_game::object_cref_type> get_games_to_resolve(fc::time_point_sec resolve_time) const override;
    std::vector<dbs_game::object_cref_type> get_games_to_auto_resolve(fc::time_point_sec resolve_time) const override;

private:
    dynamic_global_property_service_i& _dprops_service;
    betting_property_service_i& _betting_props_service;
    dba::db_accessor<game_uuid_history_object>& _uuid_hist_dba;
};
}
}
