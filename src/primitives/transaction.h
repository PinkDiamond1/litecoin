// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PRIMITIVES_TRANSACTION_H
#define BITCOIN_PRIMITIVES_TRANSACTION_H

#include <stdint.h>
#include <amount.h>
#include <script/script.h>
#include <serialize.h>
#include <uint256.h>
#include <mweb/mweb_models.h>
#include <boost/optional.hpp>
#include <boost/variant.hpp>

#include <tuple>
#include <typeinfo>

/**
 * A flag that is ORed into the protocol version to designate that a transaction
 * should be (un)serialized without witness data.
 * Make sure that this does not collide with any of the values in `version.h`
 * or with `ADDRV2_FORMAT`.
 */
static const int SERIALIZE_TRANSACTION_NO_WITNESS = 0x40000000;
static const int SERIALIZE_NO_MWEB = 0x20000000;

/** An outpoint - a combination of a transaction hash and an index n into its vout */
class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

    COutPoint(): n(NULL_INDEX) { }
    COutPoint(const uint256& hashIn, uint32_t nIn): hash(hashIn), n(nIn) { }

    SERIALIZE_METHODS(COutPoint, obj) { READWRITE(obj.hash, obj.n); }

    void SetNull() { hash.SetNull(); n = NULL_INDEX; }
    bool IsNull() const { return (hash.IsNull() && n == NULL_INDEX); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        int cmp = a.hash.Compare(b.hash);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

/** An input of a transaction.  It contains the location of the previous
 * transaction's output that it claims and a signature that matches the
 * output's public key.
 */
class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;
    CScriptWitness scriptWitness; //!< Only serialized through CTransaction

    /* Setting nSequence to this value for every input in a transaction
     * disables nLockTime. */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /* If this flag set, CTxIn::nSequence is NOT interpreted as a
     * relative lock-time. */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1U << 31);

    /* If CTxIn::nSequence encodes a relative lock-time and this flag
     * is set, the relative lock-time has units of 512 seconds,
     * otherwise it specifies blocks with a granularity of 1. */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /* If CTxIn::nSequence encodes a relative lock-time, this mask is
     * applied to extract that lock-time from the sequence field. */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /* In order to use the same number of bits to encode roughly the
     * same wall-clock duration, and because blocks are naturally
     * limited to occur every 600s on average, the minimum granularity
     * for time-based relative lock-time is fixed at 512 seconds.
     * Converting from CTxIn::nSequence to seconds is performed by
     * multiplying by 512 = 2^9, or equivalently shifting up by
     * 9 bits. */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn()
    {
        nSequence = SEQUENCE_FINAL;
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL);

    SERIALIZE_METHODS(CTxIn, obj) { READWRITE(obj.prevout, obj.scriptSig, obj.nSequence); }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

typedef boost::variant<COutPoint, mw::Hash> OutputIndex;

/// <summary>
/// A generic transaction input that could either be an MWEB input hash or a canonical CTxIn.
/// </summary>
class CTxInput
{
public:
    CTxInput(mw::Hash output_id)
        : m_input(std::move(output_id)) {}
    CTxInput(CTxIn txin)
        : m_input(std::move(txin)) {}

    bool IsMWEB() const noexcept { return m_input.type() == typeid(mw::Hash); }
    OutputIndex GetIndex() const noexcept
    {
        return IsMWEB() ? OutputIndex{ToMWEB()} : OutputIndex{GetTxIn().prevout};
    }

    std::string ToString() const
    {
        return IsMWEB() ? ToMWEB().ToHex() : GetTxIn().ToString();
    }

    const mw::Hash& ToMWEB() const noexcept
    {
        assert(IsMWEB());
        return boost::get<mw::Hash>(m_input);
    }

    const CTxIn& GetTxIn() const noexcept
    {
        assert(!IsMWEB());
        return boost::get<CTxIn>(m_input);
    }

private:
    boost::variant<CTxIn, mw::Hash> m_input;
};

/** An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;

    CTxOut()
    {
        SetNull();
    }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);

    SERIALIZE_METHODS(CTxOut, obj) { READWRITE(obj.nValue, obj.scriptPubKey); }

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull() const
    {
        return (nValue == -1);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    std::string ToString() const;
};

class CTransaction;

/// <summary>
/// A generic transaction output that could either be an MWEB output ID or a canonical CTxOut.
/// </summary>
class CTxOutput
{
public:
    CTxOutput() = default;
    CTxOutput(mw::Hash output_id)
        : m_idx(std::move(output_id)), m_txout(boost::none) {}
    CTxOutput(OutputIndex idx, CTxOut txout)
        : m_idx(std::move(idx)), m_txout(std::move(txout)) {}

    bool IsMWEB() const noexcept { return m_idx.type() == typeid(mw::Hash); }

    const OutputIndex& GetIndex() const noexcept { return m_idx; }

    std::string ToString() const
    {
        return IsMWEB() ? ToMWEB().ToHex() : GetTxOut().ToString();
    }
    
    const mw::Hash& ToMWEB() const noexcept
    {
        assert(IsMWEB());
        return boost::get<mw::Hash>(m_idx);
    }

    const CTxOut& GetTxOut() const noexcept
    {
        assert(!IsMWEB() && !!m_txout);
        return *m_txout;
    }

    const CScript& GetScriptPubKey() const noexcept
    {
        assert(!IsMWEB());
        return GetTxOut().scriptPubKey;
    }

private:
    OutputIndex m_idx;
    boost::optional<CTxOut> m_txout;
};

struct CMutableTransaction;

/**
 * Basic transaction serialization format:
 * - int32_t nVersion
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - uint32_t nLockTime
 *
 * Extended transaction serialization format:
 * - int32_t nVersion
 * - unsigned char dummy = 0x00
 * - unsigned char flags (!= 0)
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - if (flags & 1):
 *   - CTxWitness wit;
 * - uint32_t nLockTime
 */
template<typename Stream, typename TxType>
inline void UnserializeTransaction(TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);
    const bool fAllowMWEB = !(s.GetVersion() & SERIALIZE_NO_MWEB);

    s >> tx.nVersion;
    unsigned char flags = 0;
    tx.vin.clear();
    tx.vout.clear();
    /* Try to read the vin. In case the dummy is there, this will be read as an empty vector. */
    s >> tx.vin;
    if (tx.vin.size() == 0 && fAllowWitness) {
        /* We read a dummy or an empty vin. */
        s >> flags;
        if (flags != 0) {
            s >> tx.vin;
            s >> tx.vout;
        }
    } else {
        /* We read a non-empty vin. Assume a normal vout follows. */
        s >> tx.vout;
    }
    if ((flags & 1) && fAllowWitness) {
        /* The witness flag is present, and we support witnesses. */
        flags ^= 1;
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s >> tx.vin[i].scriptWitness.stack;
        }
        if (!tx.HasWitness()) {
            /* It's illegal to encode witnesses when all witness stacks are empty. */
            throw std::ios_base::failure("Superfluous witness record");
        }
    }
    if ((flags & 8) && fAllowMWEB) {
        /* The MWEB flag is present, and we support MWEB. */
        flags ^= 8;

        s >> tx.mweb_tx;
        if (tx.mweb_tx.IsNull()) {
            if (tx.vout.empty()) {
                /* It's illegal to include a HogEx with no outputs. */
                throw std::ios_base::failure("Missing HogEx output");
            }

            /* If the mw flag is set, but there are no mw txs, assume HogEx txn. */
            tx.m_hogEx = true;
        }
    }
    if (flags) {
        /* Unknown flag in the serialization */
        throw std::ios_base::failure("Unknown transaction optional data");
    }
    s >> tx.nLockTime;
}

template<typename Stream, typename TxType>
inline void SerializeTransaction(const TxType& tx, Stream& s) {
    const bool fAllowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);
    const bool fAllowMWEB = !(s.GetVersion() & SERIALIZE_NO_MWEB);

    s << tx.nVersion;
    unsigned char flags = 0;
    // Consistency check
    if (fAllowWitness) {
        /* Check whether witnesses need to be serialized. */
        if (tx.HasWitness()) {
            flags |= 1;
        }
    }
    if (fAllowMWEB) {
        if (tx.m_hogEx || !tx.mweb_tx.IsNull()) {
            flags |= 8;
        }
    }

    if (flags) {
        /* Use extended format in case witnesses are to be serialized. */
        std::vector<CTxIn> vinDummy;
        s << vinDummy;
        s << flags;
    }
    s << tx.vin;
    s << tx.vout;
    if (flags & 1) {
        for (size_t i = 0; i < tx.vin.size(); i++) {
            s << tx.vin[i].scriptWitness.stack;
        }
    }
    if (flags & 8) {
        s << tx.mweb_tx;
    }
    s << tx.nLockTime;
}


/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
public:
    // Default transaction version.
    static const int32_t CURRENT_VERSION=2;

    // Changing the default transaction version requires a two step process: first
    // adapting relay policy by bumping MAX_STANDARD_VERSION, and then later date
    // bumping the default CURRENT_VERSION at which point both CURRENT_VERSION and
    // MAX_STANDARD_VERSION will be equal.
    static const int32_t MAX_STANDARD_VERSION=2;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const int32_t nVersion;
    const uint32_t nLockTime;
    const MWEB::Tx mweb_tx;
    
    /** Memory only. */
    const bool m_hogEx;

private:
    /** Memory only. */
    const uint256 hash;
    const uint256 m_witness_hash;

    uint256 ComputeHash() const;
    uint256 ComputeWitnessHash() const;

public:
    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    explicit CTransaction(const CMutableTransaction &tx);
    CTransaction(CMutableTransaction &&tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }

    /** This deserializing constructor is provided instead of an Unserialize method.
     *  Unserialize is not possible, since it would require overwriting const fields. */
    template <typename Stream>
    CTransaction(deserialize_type, Stream& s) : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    const uint256& GetHash() const { return hash; }
    const uint256& GetWitnessHash() const { return m_witness_hash; };

    // Return sum of txouts.
    CAmount GetValueOut() const;

    /**
     * Get the total transaction size in bytes, including witness data.
     * "Total Size" defined in BIP141 and BIP144.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const;

    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return a.hash == b.hash;
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return a.hash != b.hash;
    }

    std::string ToString() const;

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }

    bool HasMWEBTx() const noexcept { return !mweb_tx.IsNull(); }
    bool IsHogEx() const noexcept { return m_hogEx; }

    /// <summary>
    /// Builds a vector of CTxInputs, starting with the canoncial inputs (CTxIn), followed by the MWEB input hashes.
    /// </summary>
    /// <returns>A vector of all of the transaction's inputs.</returns>
    std::vector<CTxInput> GetInputs() const noexcept;

    /// <summary>
    /// Constructs a CTxOutput for the specified canonical output.
    /// </summary>
    /// <param name="index">The index of the CTxOut. This must be a valid index.</param>
    /// <returns>The CTxOutput object.</returns>
    CTxOutput GetOutput(const size_t index) const noexcept;

    /// <summary>
    /// Constructs a CTxOutput for the specified output.
    /// </summary>
    /// <param name="idx">The index of the output. This could either be an output ID or a valid canonical output index.</param>
    /// <returns>The CTxOutput object.</returns>
    CTxOutput GetOutput(const OutputIndex& idx) const noexcept;

    /// <summary>
    /// Builds a vector of CTxOutputs, starting with the canoncial outputs (CTxOut), followed by the MWEB output IDs.
    /// </summary>
    /// <returns>A vector of all of the transaction's outputs.</returns>
    std::vector<CTxOutput> GetOutputs() const noexcept;
};

/** A mutable version of CTransaction. */
struct CMutableTransaction
{
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    int32_t nVersion;
    uint32_t nLockTime;
    MWEB::Tx mweb_tx;

    /** Memory only. */
    bool m_hogEx = false;

    CMutableTransaction();
    explicit CMutableTransaction(const CTransaction& tx);

    template <typename Stream>
    inline void Serialize(Stream& s) const {
        SerializeTransaction(*this, s);
    }


    template <typename Stream>
    inline void Unserialize(Stream& s) {
        UnserializeTransaction(*this, s);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream& s) {
        Unserialize(s);
    }

    /** Compute the hash of this CMutableTransaction. This is computed on the
     * fly, as opposed to GetHash() in CTransaction, which uses a cached result.
     */
    uint256 GetHash() const;

    bool HasWitness() const
    {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }

    bool HasMWEBTx() const noexcept { return !mweb_tx.IsNull(); }
};

typedef std::shared_ptr<const CTransaction> CTransactionRef;
static inline CTransactionRef MakeTransactionRef() { return std::make_shared<const CTransaction>(); }
template <typename Tx> static inline CTransactionRef MakeTransactionRef(Tx&& txIn) { return std::make_shared<const CTransaction>(std::forward<Tx>(txIn)); }

template <typename Stream>
void Unserialize(Stream& is, std::shared_ptr<const CTransaction>& p)
{
    p = std::make_shared<const CTransaction>(deserialize, is);
}

/** A generic txid reference (txid or wtxid). */
class GenTxid
{
    bool m_is_wtxid;
    uint256 m_hash;
public:
    GenTxid(bool is_wtxid, const uint256& hash) : m_is_wtxid(is_wtxid), m_hash(hash) {}
    bool IsWtxid() const { return m_is_wtxid; }
    const uint256& GetHash() const { return m_hash; }
    friend bool operator==(const GenTxid& a, const GenTxid& b) { return a.m_is_wtxid == b.m_is_wtxid && a.m_hash == b.m_hash; }
    friend bool operator<(const GenTxid& a, const GenTxid& b) { return std::tie(a.m_is_wtxid, a.m_hash) < std::tie(b.m_is_wtxid, b.m_hash); }
};

#endif // BITCOIN_PRIMITIVES_TRANSACTION_H
