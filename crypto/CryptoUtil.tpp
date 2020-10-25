namespace {
template<typename InputIt, typename OutputIt>
void safe_copy(const InputIt from, size_t count, const InputIt from_limit,
               OutputIt to, OutputIt to_limit) {
  if (from == nullptr or from_limit == nullptr or
      to == nullptr or to_limit == nullptr or
      count == 0) {
    throw std::invalid_argument("Any of arguments is either 0 or nullptr");
  }
  const auto from_end = from + count;
  const auto to_end = to + count;
  if (from_end < from or
      from_limit < from_end or
      to_end < to or
      to_limit < to_end) {
    throw std::range_error("Invalid access");
  }

  std::memcpy(to, from, count);
}
}

// FIXME: Depending on architecture the size of size_t could be different
template <typename SizeT>
zkp zkp::deserialize(const unsigned char* data, std::size_t size) {
  const std::range_error err("Invalid access");

  const unsigned char* address = data;
  const unsigned char* const address_limit = data + size;

  SizeT user_id_size = 0;
  safe_copy(address, sizeof(user_id_size), address_limit,
            &user_id_size, &user_id_size + sizeof(user_id_size));
  address += sizeof(user_id_size);
  if (user_id_size > size - std::distance(data, address)) {
    throw err;
  }

  const unsigned char* const user_id_ptr = address;
  address += user_id_size;

  SizeT V_size = 0;
  safe_copy(address, sizeof(V_size), address_limit,
            &V_size, &V_size + sizeof(V_size));
  address += sizeof(V_size);
  if (V_size > size - std::distance(data, address)) {
    throw err;
  }

  const unsigned char* const V_ptr = address;
  address += V_size;

  SizeT r_size = 0;
  safe_copy(address, sizeof(r_size), address_limit,
            &r_size, &r_size + sizeof(r_size));
  address += sizeof(r_size);
  if (r_size > size - std::distance(data, address)) {
    throw err;
  }

  const unsigned char* const r_ptr = address;
  address += r_size;

  if (address > address_limit) {
    throw err;
  }

  assert(address == data + size);

  auto result = zkp{
    std::string{user_id_ptr, user_id_ptr + user_id_size},
    [=]{
      EcPoint point = make_point();
      EC_POINT_oct2point(CurveGroup(), point.get(),
                         V_ptr, V_size, get_bnctx());
      return point;
    }(),
    make_scalar(BN_bin2bn(r_ptr, r_size, nullptr))
  };

  if (not (result.r and result.V)) {
    int_error("Failed to deserialize zkp");
  }

  return result;
}

template <typename SizeT>
std::vector<unsigned char> zkp::serialize() const {
  const auto cast_to_size = [](auto value, std::string&& name) {
    if (value > std::numeric_limits<SizeT>::max()) {
      throw std::length_error(
          "zkp::serialize(): " + name + " too long");
    }
    return SizeT(value);
  };

    unsigned char* V_buffer = nullptr;
    const SizeT V_size =
        cast_to_size(EC_POINT_point2buf(CurveGroup(), V.get(),
                                        POINT_CONVERSION_COMPRESSED,
                                        &V_buffer, get_bnctx()),
                     "V");
    if (0 == V_size) {
      int_error("Failed converting EC_POINT to the octet string");
    }

    const SizeT user_id_size = cast_to_size(user_id.size(), "user_id");

    const SizeT r_size = cast_to_size(BN_num_bytes(r.get()), "r");

    std::vector<unsigned char> result(user_id_size +
                                      V_size +
                                      r_size +
                                      3 * sizeof(SizeT));

    unsigned char* address = result.data();

    Util::safe_memcpy(address, &user_id_size, sizeof(user_id_size));
    address += sizeof(user_id_size);
    Util::safe_memcpy(address, user_id.data(), user_id_size);
    address += user_id_size;

    Util::safe_memcpy(address, &V_size, sizeof(V_size));
    address += sizeof(V_size);
    Util::safe_memcpy(address, V_buffer, V_size);
    address += V_size;

    Util::safe_memcpy(address, &r_size, sizeof(r_size));
    address += sizeof(r_size);
    BN_bn2bin(r.get(), address);

    assert(address + r_size == result.data() + result.size());

    return result;
  }
