#include "trail.service.hpp"

trail::trail(account_name self) : contract(self), env_singleton(self, self) {
    if (!env_singleton.exists()) {

        env_struct = environment{
            self, //publisher
            0, //total_tokens
            0, //total_voters
            0 //total_ballots
        };

        env_singleton.set(env_struct, self);
    } else {
        env_struct = env_singleton.get();
    }
}

trail::~trail() {
    if (env_singleton.exists()) {
        env_singleton.set(env_struct, env_struct.publisher);
    }
}

#pragma region Tokens

void trail::regtoken(asset native, account_name publisher) {
    require_auth(publisher);

    auto sym = native.symbol.name();

    stats statstable(N(eosio.token), sym);
    auto eosio_existing = statstable.find(sym);
    eosio_assert(eosio_existing == statstable.end(), "Token with symbol already exists in eosio.token" );

    registries_table registries(_self, sym);
    auto r = registries.find(sym);
    eosio_assert(r == registries.end(), "Token Registry with that symbol already exists in Trail");

    registries.emplace(publisher, [&]( auto& a ){
        a.native = native;
        a.publisher = publisher;
    });

    env_struct.total_tokens++;

    print("\nToken Registration: SUCCESS");
}

void trail::unregtoken(asset native, account_name publisher) {
    require_auth(publisher);
    
    auto sym = native.symbol.name();
    registries_table registries(_self, sym);
    auto r = registries.find(sym);

    eosio_assert(r != registries.end(), "Token Registry does not exist.");

    registries.erase(r);

    env_struct.total_tokens--;

    print("\nToken Unregistration: SUCCESS");
}

#pragma endregion Trail_Tokens

#pragma region Voting

void trail::regvoter(account_name voter) {
    require_auth(voter);

    voters_table voters(_self, voter);
    auto v = voters.find(voter);

    eosio_assert(v == voters.end(), "Voter already exists");

    voters.emplace(voter, [&]( auto& a ){
        a.voter = voter;
    });

    env_struct.total_voters++;

    print("\nVoterID Registration: SUCCESS");
}

void trail::unregvoter(account_name voter) {
    require_auth(voter);

    voters_table voters(_self, voter);
    auto v = voters.find(voter);

    eosio_assert(v != voters.end(), "Voter Doesn't Exist");

    auto vid = *v;  

    voters.erase(v);

    env_struct.total_voters--;

    print("\nVoterID Unregistration: SUCCESS");
}

void trail::regballot(account_name publisher) {
    require_auth(publisher);

    ballots_table ballots(_self, publisher);
    auto b = ballots.find(publisher);

    eosio_assert(b == ballots.end(), "Ballot already exists");

    ballots.emplace(publisher, [&]( auto& a ){
        a.publisher = publisher;
    });

    env_struct.total_ballots++;

    print("\nBallot Registration: SUCCESS");
}

void trail::unregballot(account_name publisher) {
    require_auth(publisher);

    ballots_table ballots(_self, publisher);
    auto b = ballots.find(publisher);

    eosio_assert(b != ballots.end(), "Ballot Doesn't Exist");

    ballots.erase(b);

    env_struct.total_ballots--;

    print("\nBallot Unregistration: SUCCESS");
}

#pragma endregion Voting

//EOSIO_ABI(trail, (regtoken)(unregtoken)(regvoter)(unregvoter)(addreceipt)(rmvexpvotes)(regballot)(unregballot))

extern "C" {
    void apply(uint64_t self, uint64_t code, uint64_t action) {
        trail _trail(self);
        if(code == self && action == N(regtoken)) {
            execute_action(&_trail, &trail::regtoken);
        } else if (code == self && action == N(unregtoken)) {
            execute_action(&_trail, &trail::unregtoken);
        } else if (code==self && action==N(regvoter)) {
            execute_action(&_trail, &trail::regvoter);
        } else if (code == self && action == N(unregvoter)) {
            execute_action(&_trail, &trail::unregvoter);
        } else if (code == self && action == N(regballot)) {
            execute_action(&_trail, &trail::regballot);
        } else if (code == self && action == N(unregballot)) {
            execute_action(&_trail, &trail::unregballot);
        } else if (code == N(eosio) && action == N(delegatebw)) { //TODO: add undelegatebw()

            auto args = unpack_action_data<delegatebw_args>();
            asset new_weight = (args.stake_cpu_quantity + args.stake_net_quantity);

            receipts_table votereceipts(self, self);
            auto by_voter = votereceipts.get_index<N(byvoter)>();
            auto itr = by_voter.lower_bound(args.from);

            while(itr->voter == args.from) {
                if (now() <= itr->expiration) {
                    by_voter.modify(itr, 0, [&]( auto& a ) {
                        a.weight += new_weight;
                    });
                    print("\npropagated weight change to id: ", itr->receipt_id);
                }
                itr++;
            }

        } else if (code == N(eosio) && action == N(undelegatebw)) {
            //TODO: implement
        } else if (code == N(eosio.amend) && action == N(vote)) { //TODO: change code to be any registered ballot
            auto args = unpack_action_data<vote_args>();

            receipts_table votereceipts(self, self);
            auto by_voter = votereceipts.get_index<N(byvoter)>();
            auto itr = by_voter.lower_bound(args.voter);
            asset new_weight = get_staked_tlos(args.voter);

            while(itr->voter == args.voter) {
                if (now() <= itr->expiration && 
                    itr->vote_code == args.vote_code && 
                    itr->vote_scope == args.vote_scope && 
                    itr->proposal_id == args.proposal_id) {

                    by_voter.modify(itr, 0, [&]( auto& a ) {
                        a.direction = args.direction;
                        a.weight = new_weight;
                    });
                    print("\nVR found and updated");

                }
                itr++;
            }

        } else if (code == N(eosio.amend) && action == N(processvotes)) {
            //TODO: implement
        }
    } //end apply
};
