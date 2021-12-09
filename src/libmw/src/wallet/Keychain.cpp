#include <mw/wallet/Keychain.h>
#include <mw/crypto/Blinds.h>
#include <mw/crypto/Hasher.h>
#include <mw/models/tx/OutputMask.h>
#include <wallet/scriptpubkeyman.h>

MW_NAMESPACE

bool Keychain::RewindOutput(const Output& output, mw::Coin& coin) const
{
    SecretKey t = Hashed(EHashTag::DERIVE, output.Ke().Mul(GetScanSecret()));
    if (t[0] != output.GetViewTag()) {
        return false;
    }

    PublicKey B_i = output.Ko().Div(Hashed(EHashTag::OUT_KEY, t));

    // Check if B_i belongs to wallet
    StealthAddress address(B_i.Mul(m_scanSecret), B_i);
    auto pMetadata = m_spk_man.GetMetadata(address);
    if (!pMetadata || !pMetadata->mweb_index) {
        return false;
    }

    uint32_t index = *pMetadata->mweb_index;

    // Calc blinding factor and unmask nonce and amount.
    OutputMask mask = OutputMask::FromShared(t);
    uint64_t value = mask.MaskValue(output.GetMaskedValue());
    BigInt<16> n = mask.MaskNonce(output.GetMaskedNonce());

    if (mask.SwitchCommit(value) != output.GetCommitment()) {
        return false;
    }

    // Calculate Carol's sending key 's' and check that s*B ?= Ke
    StealthAddress wallet_addr = GetStealthAddress(index);
    SecretKey s = Hasher(EHashTag::SEND_KEY)
                      .Append(wallet_addr.A())
                      .Append(wallet_addr.B())
                      .Append(value)
                      .Append(n)
                      .hash();
    if (output.Ke() != wallet_addr.B().Mul(s)) {
        return false;
    }

    SecretKey private_key = Blinds()
        .Add(Hashed(EHashTag::OUT_KEY, t))
        .Add(GetSpendKey(index))
        .ToKey();

    coin.address_index = index;
    coin.key = boost::make_optional(std::move(private_key));
    coin.blind = boost::make_optional(mask.GetRawBlind());
    coin.amount = value;
    coin.commitment = output.GetCommitment();

    return true;
}

StealthAddress Keychain::GetStealthAddress(const uint32_t index) const
{
    PublicKey Bi = PublicKey::From(GetSpendKey(index));
    PublicKey Ai = Bi.Mul(m_scanSecret);

    return StealthAddress(Ai, Bi);
}

SecretKey Keychain::GetSpendKey(const uint32_t index) const
{
    SecretKey mi = Hasher(EHashTag::ADDRESS)
        .Append<uint32_t>(index)
        .Append(m_scanSecret)
        .hash();

    return Blinds().Add(m_spendSecret).Add(mi).ToKey();
}

END_NAMESPACE