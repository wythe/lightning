#include <common/wallet_tx.h>
#include <inttypes.h>
#include <lightningd/jsonrpc_errors.h>
#include <wallet/wallet.h>

void wtx_init(struct command *cmd, struct wallet_tx * wtx)
{
	wtx->cmd = cmd;
	wtx->amount = 0;
	wtx->change_key_index = 0;
	wtx->utxos = NULL;
	wtx->all_funds = false;
}

static bool check_amount(const struct wallet_tx *tx, u64 amount)
{
	if (!tx->utxos) {
		command_fail(tx->cmd, FUND_CANNOT_AFFORD,
			     "Cannot afford transaction");
		return false;
	}
	if (amount < 546) {
		command_fail(tx->cmd, FUND_OUTPUT_IS_DUST,
			     "Output %"PRIu64" satoshis would be dust",
			     amount);
		return false;
	}
	return true;
}

bool wtx_select_utxos(struct wallet_tx * tx, u32 fee_rate_per_kw,
	              size_t out_len)
{
	u64 fee_estimate;
	u64 satoshi_in;
	u64 amount;
	u64 ask = tx->all_funds ? (1ULL << 56) : tx->amount;

	if (tx->all_funds) {

		tx->utxos = wallet_select(tx->cmd, tx->cmd->ld->wallet,
				     ask, fee_rate_per_kw,
				     out_len, !tx->all_funds,
				     &satoshi_in, &fee_estimate);

		/* Can't afford fees? */
		if (fee_estimate > satoshi_in)
			return tal_free(tx->utxos);

		amount = satoshi_in - fee_estimate;

		if (!check_amount(tx, amount))
			return false;

		if (amount <= tx->amount) {
			tx->change = 0;
			tx->amount = amount;
			return true;
		}

		/* Too much?  Try again, but ask for limit instead. */
		tx->all_funds = false;
		tx->utxos = tal_free(tx->utxos);
	}

	tx->utxos = wallet_select(tx->cmd, tx->cmd->ld->wallet,
			     ask, fee_rate_per_kw,
			     out_len, !tx->all_funds,
			     &satoshi_in, &fee_estimate);

	/* Couldn't afford it? */
	if (satoshi_in < fee_estimate + tx->amount)
		tx->utxos = tal_free(tx->utxos);
	else
		tx->change = satoshi_in - tx->amount - fee_estimate;

	if (!check_amount(tx, tx->amount))
		return false;

	if (tx->change < 546) {
		tx->change = 0;
		tx->change_key_index = 0;
	} else {
		tx->change_key_index = wallet_get_newindex(tx->cmd->ld);
	}
	return true;
}
