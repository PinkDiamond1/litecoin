# MWEB Consensus

## Prerequisites

### Notation

* <code>H(&alpha;||&beta;||&gamma;)</code> represents the standard 32-byte BLAKE3 hash of the conjoined serializations of <code>&alpha;</code>, <code>&beta;</code>, and <code>&gamma;</code>.

### Transaction Structure

##### Transaction

* <code>x</code> = kernel offset
* <code>x'</code> = stealth offset

##### Inputs

* <code>O<sub>ID</sub></code> = ID of the output being spent
* <code>C<sub>i</sub></code> = Input commitment (<code>C<sub>O</sub></code> of the output being spent)
* <code>K<sub>i</sub></code> = Input stealth public key
* <code>&sigma;</code> = Input signature

##### Kernels

* <code>M</code> = Type, fee, etc.
* <code>E</code> = Kernel excess
* <code>E'</code> = (Optional) Stealth excess
* <code>&psi;</code> = Kernel signature

##### Outputs

* <code>C<sub>O</sub></code> = Output commitment
* <code>K<sub>s</sub></code> = Sender public key 
* <code>K<sub>O</sub></code> = Output public key
* <code>K<sub>e</sub></code> = Key exchange public key
* <code>t[0]</code> = View tag
* <code>v'</code> = Encrypted value
* <code>n'</code> = Encrypted nonce
* <code>&rho;</code> = Output signature



## Rules

#### Identifiers

* The kernel ID (<code>K<sub>ID</sub></code>) is the BLAKE3 hash of the serialized kernel.
  * See [kernels.md](./kernels.md) for the kernel serialization format.
* The output ID (<code>O<sub>ID</sub></code>) is the BLAKE3 hash of the serialized output.
  * <code>O<sub>ID</sub> = H(C<sub>O</sub>||H(O<sub>M</sub>)||H(&pi;)||&rho;)</code>
---

#### Transaction Uniqueness

* Output hashes (<code>O<sub>ID</sub></code>) in the UTXO set shall be unique at all times.
* A block shall not be included when it contains an output ID (<code>O<sub>ID</sub></code>) which already exists in the UTXO set.
  * Block is still invalid even if it also contains an input with said output ID (<code>O<sub>ID</sub></code>).
* A block shall not contain 2 outputs with the same ID (<code>O<sub>ID</sub></code>).
  * Block is still invalid even if it also contains an input with said output ID (<code>O<sub>ID</sub></code>).
* A block *could* contain an input which spends an output created in the same block, provided the output ID (<code>O<sub>ID</sub></code>) was not already in the UTXO set.
  * To better support payment proofs, the outputs shall still be added to the TXO MMR, but shall be marked as spent.
* A block shall not contain 2 kernels with the same ID (<code>K<sub>ID</sub></code>).
---

#### Ordering

* Inputs in a block shall be in ascending order by output ID (<code>O<sub>ID</sub></code>).
* Outputs in a block shall be in ascending order by output ID (<code>O<sub>ID</sub></code>).
* Kernels in a block shall list pegins first, and then be ordered in ascending order by kernel ID (<code>K<sub>ID</sub></code>).
---

#### PMMRs

* After building an MMR with all of the kernels from a block, the root shall match the header's kernel root.
* The number of kernels in a block shall match the header's kernel size.
* After adding all outputs from a block to the end of the output PMMR, the root shall match the header's output root.
* After adding all outputs from a block to the end of the output PMMR, the size shall match the header's output size.
---

#### UTXO LeafSet

* A simple bitset shall be created and maintained to keep track of which TXOs in the output PMMR remain unspent.
* The bit positions shall map 1-to-1 to to PMMR leaf indices. A `0` at that bit position means spent, whereas a `1` means unspent. 
  * Ex: If byte 0 bit 2 is a `1`, that means TXO at leaf index `2` is unspent. If byte 2 bit 1 is a `0`, the TXO at leaf index `17` has been spent. etc.
* The hash of the serialized UTXO set after applying the transactions in a block shall match the header's UTXO leafset hash.
---

#### Signatures

* Each kernel without a stealth excess (`E'`) shall have a valid signature (&psi;) of <code>H(K<sub>M</sub>)</code> that verifies for the kernel's excess(`E`).
  * See [kernels.md](./kernels.md) for the signature message (<code>K<sub>M</sub></code>) serialization format
* Each kernel with a stealth excess (`E'`) shall have a valid signature (&psi;) of <code>H(K<sub>M</sub>)</code> for key <code>E<sub>agg</sub> = H(E||E')\*E + E'</code>.
* Each input shall have a valid signature (&sigma;) of the ID (<code>O<sub>ID</sub></code>) of the output it spends for key <code>K<sub>agg</sub> = K<sub>i</sub> + H(K<sub>i</sub>||K<sub>o</sub>)\*K<sub>o</sub></code>.
* Each output shall have a valid signature (&rho;) of <code>(C<sub>O</sub>||O<sub>M</sub>||H(&pi;))</code> for the output's sender pubkey (<code>K<sub>s</sub></code>).
  * `output_message` (<code>O<sub>M</sub></code>) is serialized as <code>O<sub>M</sub> = (K<sub>o</sub>||K<sub>e</sub>||t[0]||v'||n'||K<sub>s</sub>)</code>
---

#### Bulletproofs

* Each output shall be coupled with a bulletproof that proves the commitment is to a value in the range `[0, 2^64)`.
* Each bulletproof (<code>&pi;</code>) shall commit to the `output_message` using its `extra_commit` functionality.
---

#### Kernel Sums

* The sum of all output commitments (<code>C<sub>O</sub></code>) in the UTXO set at a given block height shall equal the sum of all kernel commitments (`E`) plus the `total_kernel_offset*G` (`x*G`) and the `expected_supply*H` (<code>v<sup>T</sup></code>) of the block.
  * <code>&Sigma;C<sub>O</sub> = &Sigma;E + (x\*G) + (v<sup>T</sup>\*H)</code>
---

#### Stealth Sums

* The sum of all sender pubkeys (<code>K<sub>s</sub></code>) in a block and all input pubkeys (<code>K<sub>i</sub></code>) in the block must equal the sum of all stealth excesses (`E'`) plus `total_stealth_offset*G` (`x'*G`) plus the sum of all output pubkeys (<code>K<sub>o</sub></code>) being spent in the block.
  * <code>&Sigma;K<sub>s</sub> + &Sigma;K<sub>i</sub> = &Sigma;E' + x'\*G + &Sigma;K<sub>o</sub></code>
---

#### Pegging-In && Pegging-Out

* Kernels may include an optional pegout, containing the `amount` & a `scriptPubKey` (serialized `CScript`)
  * The `scriptPubKey` for a pegout shall be a valid `CScript` between 4 and 42 bytes, inclusive.
  * Miners shall include a matching output in a block's `HogEx` transaction with the exact `amount` and `scriptPubKey` for each pegout in the block.
* Kernels may include an optional pegin `amount`
  * Each pegin kernel must have a corresponding pegin output on the canonical LTC side where the value is <code>v<sub>pegin</sub></code>, witness version is 9, and witness program is the kernel ID (<code>K<sub>ID</sub></code>).
* The total MWEB supply (<code>v<sup>T</sup></code>) shall increase by the sum of the block's pegins (<code>v<sub>pegin</sub></code>), and decrease by the sum of the block's pegouts (<code>v<sub>pegout</sub></code>) and the sum of all fees (`f`).
  * <code>v<sup>T</sup><sub>new</sub> = v<sup>T</sup><sub>prev</sub> + &Sigma;v<sub>pegin</sub> - (&Sigma;v<sub>pegout</sub> + &Sigma;f)</code>
---

#### Block Weight

* Outputs shall be counted as having a weight of 18.
* Kernels shall be counted as having a weight of either 2 (without stealth excess) or 3 (with stealth excess).
  * The `scriptPubKey` shall be a valid `CScript` between 4 and 42 bytes, inclusive.
  * Kernel `extra_data` shall not exceed 100 bytes.
* Extension blocks shall be capped at a maximum total weight of 21,000.
* Inputs shall not contribute toward the block weight.