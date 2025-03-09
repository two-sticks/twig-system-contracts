void native::init(unsigned_int version, const symbol & core){
  require_auth(get_self());
  check(version.value == 0, "unsupported version for init action");

  auto system_token_supply = token::get_supply(token_account, core.code());
  check(system_token_supply.symbol == core, "specified core symbol does not exist (precision mismatch)");

  check(system_token_supply.amount > 0, "system token supply must be greater than 0");
  _rammarket.emplace(get_self(), [&](auto& m){
    m.supply.amount = 100000000000000ll;
    m.supply.symbol = ramcore_symbol;
    m.base.balance.amount = int64_t(_gstate.free_ram());
    m.base.balance.symbol = ram_symbol;
    m.quote.balance.amount = system_token_supply.amount / 1000;
    m.quote.balance.symbol = core;
  });

  token::open_action open_act{ token_account, { {get_self(), active_permission} } };
  open_act.send(rex_account, core, get_self());
}