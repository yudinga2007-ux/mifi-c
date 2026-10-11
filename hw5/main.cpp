#include <SQLiteCpp/SQLiteCpp.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace procurement {

namespace errors {

struct DomainError : std::runtime_error {
    using runtime_error::runtime_error;
};
struct InvalidIdentifier : DomainError {
    std::string kind;
    std::int64_t attempted;
    InvalidIdentifier(std::string kind, std::int64_t attempted)
        : DomainError(std::format("недопустимый идентификатор {}: {}", kind, attempted)),
          kind(std::move(kind)), attempted(attempted) {}
};
struct InvalidRequestId : InvalidIdentifier {
    InvalidRequestId(std::int64_t v) : InvalidIdentifier("запроса", v) {}
};
struct InvalidLineId : InvalidIdentifier {
    InvalidLineId(std::int64_t v) : InvalidIdentifier("строки", v) {}
};
struct InvalidAuthorId : InvalidIdentifier {
    InvalidAuthorId(std::int64_t v) : InvalidIdentifier("автора", v) {}
};
struct InvalidProductCode : DomainError {
    std::string code;
    InvalidProductCode(std::string code)
        : DomainError(std::format("недопустимый код товара: {}", code)),
          code(std::move(code)) {}
};
struct InvalidProductPrice : DomainError {
    std::string code;
    std::int64_t kopecks;
    InvalidProductPrice(std::string code, std::int64_t kopecks)
        : DomainError(std::format("неположительная цена товара: {}", code)),
          code(std::move(code)), kopecks(kopecks) {}
};
struct InvalidQuantity : DomainError {
    int attempted;
    InvalidQuantity(int attempted)
        : DomainError(std::format("количество вне диапазона [1, 1000]: {}", attempted)),
          attempted(attempted) {}
};
struct InvalidMoney : DomainError {
    std::int64_t kopecks;
    InvalidMoney(std::int64_t kopecks)
        : DomainError(std::format("отрицательная сумма: {}", kopecks)),
          kopecks(kopecks) {}
};
struct MoneyOverflow : DomainError {
    std::int64_t unit_kopecks;
    int quantity;
    MoneyOverflow(std::int64_t unit_kopecks, int quantity)
        : DomainError("переполнение при умножении цены товара на количество"),
          unit_kopecks(unit_kopecks), quantity(quantity) {}
};
struct DuplicateLineId : DomainError {
    std::int64_t request_id, line_id;
    DuplicateLineId(std::int64_t request_id, std::int64_t line_id)
        : DomainError(std::format("дублирующийся идентификатор строки: {}", line_id)),
          request_id(request_id), line_id(line_id) {}
};
struct DuplicateProduct : DomainError {
    std::int64_t request_id;
    std::string code;
    DuplicateProduct(std::int64_t request_id, std::string code)
        : DomainError(std::format("дублирующийся код товара: {}", code)),
          request_id(request_id), code(std::move(code)) {}
};
struct RequestLimitExceeded : DomainError {
    std::int64_t request_id, current_kopecks, additional_kopecks, limit_kopecks;
    RequestLimitExceeded(std::int64_t request_id, std::int64_t current,
                         std::int64_t additional, std::int64_t limit)
        : DomainError(std::format("превышен лимит заявки: {}", request_id)),
          request_id(request_id), current_kopecks(current),
          additional_kopecks(additional), limit_kopecks(limit) {}
};
struct RequestNotDraft : DomainError {
    std::int64_t request_id;
    int actual_status;
    RequestNotDraft(std::int64_t request_id, int status)
        : DomainError(std::format("заявка не является черновиком: {}", request_id)),
          request_id(request_id), actual_status(status) {}
};
struct EmptyRequest : DomainError {
    std::int64_t request_id;
    EmptyRequest(std::int64_t id)
        : DomainError(std::format("пустая заявка: {}", id)), request_id(id) {}
};
struct CancellationDenied : DomainError {
    std::int64_t request_id, actor_id, author_id;
    CancellationDenied(std::int64_t request_id, std::int64_t actor, std::int64_t author)
        : DomainError(std::format("только автор может отменить заявку: {}", request_id)),
          request_id(request_id), actor_id(actor), author_id(author) {}
};
struct RequestNotSubmitted : DomainError {
    std::int64_t request_id;
    int actual_status;
    RequestNotSubmitted(std::int64_t id, int status)
        : DomainError(std::format("заявка не отправлена: {}", id)),
          request_id(id), actual_status(status) {}
};
struct InvalidRequestStatus : DomainError {
    std::int64_t request_id;
    int raw_status;
    InvalidRequestStatus(std::int64_t id, int status)
        : DomainError(std::format("недопустимый статус заявки: {}", id)),
          request_id(id), raw_status(status) {}
};
struct LineNotFound : DomainError {
    std::int64_t request_id, line_id;
    LineNotFound(std::int64_t request_id, std::int64_t line_id)
        : DomainError(std::format("строка не найдена: {}", line_id)),
          request_id(request_id), line_id(line_id) {}
};
struct InvalidBudgetPeriod : DomainError {
    int year, month;
    InvalidBudgetPeriod(int year, int month)
        : DomainError(std::format("недопустимый бюджетный период: {}-{}", year, month)),
          year(year), month(month) {}
};
struct BudgetExceeded : DomainError {
    std::string period;
    std::int64_t consumed_kopecks, amount_kopecks, limit_kopecks;
    BudgetExceeded(std::string period, std::int64_t consumed,
                   std::int64_t amount, std::int64_t limit)
        : DomainError(std::format("превышен месячный бюджет периода {}", period)),
          period(std::move(period)), consumed_kopecks(consumed),
          amount_kopecks(amount), limit_kopecks(limit) {}
};
struct InvalidBudgetRelease : DomainError {
    std::string period;
    std::int64_t consumed_kopecks, amount_kopecks;
    InvalidBudgetRelease(std::string period, std::int64_t consumed, std::int64_t amount)
        : DomainError(std::format("освобождение больше зарезервированного в периоде {}", period)),
          period(std::move(period)), consumed_kopecks(consumed),
          amount_kopecks(amount) {}
};
struct RepositoryError : std::runtime_error {
    using runtime_error::runtime_error;
};
struct RequestNotFound : RepositoryError {
    std::int64_t request_id;
    RequestNotFound(std::int64_t id)
        : RepositoryError(std::format("заявка не найдена: {}", id)), request_id(id) {}
};
struct RequestAlreadyExists : RepositoryError {
    std::int64_t request_id;
    RequestAlreadyExists(std::int64_t id)
        : RepositoryError(std::format("заявка уже существует: {}", id)), request_id(id) {}
};
struct InvalidSnapshot : RepositoryError {
    std::int64_t request_id;
    std::string field;
    std::string reason;
    InvalidSnapshot(std::int64_t id, std::string field, std::string reason = {})
        : RepositoryError(std::format("недопустимое сохранённое поле {} в заявке {}: {}",
                                      field, id, reason)),
          request_id(id), field(std::move(field)), reason(std::move(reason)) {}
};
struct BudgetNotFound : RepositoryError {
    std::string period;
    BudgetNotFound(std::string period)
        : RepositoryError(std::format("бюджет не найден: {}", period)),
          period(std::move(period)) {}
};
struct BudgetAlreadyExists : RepositoryError {
    std::string period;
    BudgetAlreadyExists(std::string period)
        : RepositoryError(std::format("бюджет уже существует: {}", period)),
          period(std::move(period)) {}
};
struct InvalidBudgetSnapshot : RepositoryError {
    std::string period;
    std::string reason;
    InvalidBudgetSnapshot(std::string period, std::string reason)
        : RepositoryError(std::format("недопустимый снимок бюджета {}: {}", period, reason)),
          period(std::move(period)), reason(std::move(reason)) {}
};
struct LockPathInvalid : RepositoryError {
    std::filesystem::path path;
    LockPathInvalid(std::filesystem::path path)
        : RepositoryError(std::format("путь блокировки не является каталогом: {}", path.string())),
          path(std::move(path)) {}
};
struct LockTimeout : RepositoryError {
    std::filesystem::path path;
    LockTimeout(std::filesystem::path path)
        : RepositoryError(std::format("истекло время ожидания блокировки: {}", path.string())),
          path(std::move(path)) {}
};
struct FileIoError : RepositoryError {
    std::filesystem::path path;
    std::string operation;
    FileIoError(std::filesystem::path path, std::string operation)
        : RepositoryError(std::format("ошибка операции с файлом ({}): {}", operation, path.string())),
          path(std::move(path)), operation(std::move(operation)) {}
};

}

namespace values {

struct RequestId {
    std::int64_t value;
    RequestId(std::int64_t v) : value(v) {
        if (v <= 0) throw errors::InvalidRequestId(v);
    }
};
struct LineId {
    std::int64_t value;
    LineId(std::int64_t v) : value(v) {
        if (v <= 0) throw errors::InvalidLineId(v);
    }
};
struct AuthorId {
    std::int64_t value;
    AuthorId(std::int64_t v) : value(v) {
        if (v <= 0) throw errors::InvalidAuthorId(v);
    }
};
struct BudgetPeriod {
    int year;
    int month;
    BudgetPeriod(int year, int month) : year(year), month(month) {
        if (year < 1 || month < 1 || month > 12)
            throw errors::InvalidBudgetPeriod(year, month);
    }
    std::string key() const { return std::format("{:04}-{:02}", year, month); }
    bool operator==(const BudgetPeriod &other) const {
        return year == other.year && month == other.month;
    }
};
class Quantity {
    int count;
public:
    Quantity(int count) : count(count) {
        if (count < 1 || count > 1000) throw errors::InvalidQuantity(count);
    }
    int get_count() const { return count; }
    bool operator==(const Quantity &other) const { return count == other.count; }
};
class Money {
    std::int64_t kopecks;
public:
    constexpr Money(std::int64_t kopecks) : kopecks(kopecks) {
        if (kopecks < 0) throw errors::InvalidMoney(kopecks);
    }
    std::int64_t get_kopecks() const { return kopecks; }
    Money operator*(Quantity quantity) const {
        if (kopecks > std::numeric_limits<std::int64_t>::max() / quantity.get_count())
            throw errors::MoneyOverflow(kopecks, quantity.get_count());
        return {kopecks * quantity.get_count()};
    }
    bool operator==(const Money &other) const { return kopecks == other.kopecks; }
    bool operator>(const Money &other) const { return kopecks > other.kopecks; }
    Money operator+(const Money &other) const { return {kopecks + other.kopecks}; }
    Money operator-(const Money &other) const { return {kopecks - other.kopecks}; }
};
class Product {
    std::string code;
    Money price;
    static bool valid_code_characters(const std::string &code) {
        return std::all_of(code.begin(), code.end(), [](unsigned char c) {
            return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        });
    }
public:
    Product(std::string code, Money price) : code(std::move(code)), price(price) {
        if (this->code.empty() || this->code.size() > 16 ||
            !valid_code_characters(this->code))
            throw errors::InvalidProductCode(this->code);
        if (price == Money{0})
            throw errors::InvalidProductPrice(this->code, price.get_kopecks());
    }
    const std::string &get_code() const { return code; }
    Money get_price() const { return price; }
    bool operator==(const Product &other) const {
        return code == other.code && price == other.price;
    }
};
constexpr Money operator""_kop(unsigned long long kop) {
    return {static_cast<std::int64_t>(kop)};
}
constexpr Money operator""_RUB(long double rub) {
    return {static_cast<std::int64_t>(rub * 100)};
}
constexpr Money operator""_RUB(unsigned long long rub) {
    return {static_cast<std::int64_t>(rub * 100)};
}
enum class RequestStatus { draft = 0, submitted = 1, cancelled = 2 };

}

namespace entities {

using namespace values;

class RequestLine {
    LineId id;
    Product product;
    Quantity quantity;
public:
    RequestLine(LineId id, Product product, Quantity quantity)
        : id(id), product(std::move(product)), quantity(quantity) {}
    LineId get_id() const { return id; }
    const Product &get_product() const { return product; }
    Quantity get_quantity() const { return quantity; }
    Money calculate_amount() const { return product.get_price() * quantity; }
    void change_quantity(Quantity new_quantity) { quantity = new_quantity; }
};

class PurchaseRequest {
    RequestId id;
    AuthorId author;
    RequestStatus status = RequestStatus::draft;
    std::vector<RequestLine> lines;
    static constexpr Money limit = 100'000.0_RUB;

    void require_draft() const {
        if (status != RequestStatus::draft)
            throw errors::RequestNotDraft(id.value, static_cast<int>(status));
    }
    void ensure_addition_fits(Money current, Money additional) const {
        if (additional > limit - current)
            throw errors::RequestLimitExceeded(id.value, current.get_kopecks(),
                                               additional.get_kopecks(), limit.get_kopecks());
    }
public:
    PurchaseRequest(RequestId id, AuthorId author) : id(id), author(author) {}
    RequestId get_id() const { return id; }
    AuthorId get_author() const { return author; }
    RequestStatus get_status() const { return status; }
    const std::vector<RequestLine> &get_lines() const { return lines; }

    bool contains(LineId id) const {
        return std::any_of(lines.begin(), lines.end(),
                           [id](const RequestLine &l) { return l.get_id().value == id.value; });
    }
    bool has_product(const std::string &code) const {
        return std::any_of(lines.begin(), lines.end(),
                           [&code](const RequestLine &l) { return l.get_product().get_code() == code; });
    }
    Money total() const {
        Money sum = 0;
        for (const auto &line : lines) {
            auto amount = line.calculate_amount();
            if (amount > limit - sum)
                throw errors::RequestLimitExceeded(id.value, sum.get_kopecks(),
                                                   amount.get_kopecks(), limit.get_kopecks());
            sum = sum + amount;
        }
        return Money(sum);
    }
    void add_line(LineId id, Product product, Quantity quantity) {
        require_draft();
        if (contains(id)) throw errors::DuplicateLineId(id.value, id.value);
        if (has_product(product.get_code()))
            throw errors::DuplicateProduct(id.value, product.get_code());
        RequestLine candidate(id, std::move(product), quantity);
        ensure_addition_fits(total(), candidate.calculate_amount());
        lines.push_back(std::move(candidate));
    }
    void change_quantity(LineId id, Quantity next) {
        require_draft();
        auto it = std::find_if(lines.begin(), lines.end(), [id](const RequestLine &l) {
            return l.get_id().value == id.value;
        });
        if (it == lines.end()) throw errors::LineNotFound(id.value, id.value);
        auto remainder = total() - it->calculate_amount();
        ensure_addition_fits(remainder, it->get_product().get_price() * next);
        it->change_quantity(next);
    }
    void remove_line(LineId line_id) {
        require_draft();
        auto it = std::find_if(lines.begin(), lines.end(), [line_id](const RequestLine &l) {
            return l.get_id().value == line_id.value;
        });
        if (it == lines.end()) throw errors::LineNotFound(id.value, line_id.value);
        lines.erase(it);
    }
    void submit() {
        require_draft();
        if (lines.empty()) throw errors::EmptyRequest(id.value);
        status = RequestStatus::submitted;
    }
    void cancel(AuthorId actor) {
        if (actor.value != author.value)
            throw errors::CancellationDenied(id.value, actor.value, author.value);
        if (status != RequestStatus::submitted)
            throw errors::RequestNotSubmitted(id.value, static_cast<int>(status));
        status = RequestStatus::cancelled;
    }
    static PurchaseRequest restore(RequestId id, AuthorId author, RequestStatus status,
                                   const std::vector<RequestLine> &rows) {
        PurchaseRequest request(id, author);
        for (const auto &row : rows)
            request.add_line(row.get_id(), row.get_product(), row.get_quantity());
        request.status = status;
        return request;
    }
};

class MonthlyBudget {
    BudgetPeriod period;
    Money limit;
    Money consumed{0};
public:
    MonthlyBudget(BudgetPeriod period, Money limit) : period(period), limit(limit) {}
    BudgetPeriod get_period() const { return period; }
    Money get_limit() const { return limit; }
    Money get_consumed() const { return consumed; }
    Money remaining() const { return limit - consumed; }
    void reserve(Money amount) {
        if (amount > remaining())
            throw errors::BudgetExceeded(period.key(), consumed.get_kopecks(),
                                         amount.get_kopecks(), limit.get_kopecks());
        consumed = consumed + amount;
    }
    void release(Money amount) {
        if (amount > consumed)
            throw errors::InvalidBudgetRelease(period.key(), consumed.get_kopecks(),
                                               amount.get_kopecks());
        consumed = consumed - amount;
    }
    static MonthlyBudget restore(BudgetPeriod period, Money limit, Money consumed) {
        MonthlyBudget budget(period, limit);
        if (consumed > limit)
            throw errors::InvalidBudgetSnapshot(period.key(), "потрачено больше лимита");
        budget.consumed = consumed;
        return budget;
    }
};

}

namespace domain {
using namespace entities;
}

namespace ports {

using RequestChange = std::function<void(domain::PurchaseRequest &)>;
struct RequestUpdater {
    virtual void update(domain::RequestId id, const RequestChange &change) = 0;
    virtual ~RequestUpdater() = default;
};
struct PurchaseRequestRepository : RequestUpdater {
    virtual std::optional<domain::PurchaseRequest> load(domain::RequestId id) = 0;
    virtual void create(const domain::PurchaseRequest &request) = 0;
    virtual ~PurchaseRequestRepository() = default;
};
using BudgetChange = std::function<void(domain::MonthlyBudget &)>;
struct BudgetUpdater {
    virtual void update(values::BudgetPeriod period, const BudgetChange &change) = 0;
    virtual ~BudgetUpdater() = default;
};
struct MonthlyBudgetRepository : BudgetUpdater {
    virtual std::optional<domain::MonthlyBudget> load(values::BudgetPeriod period) = 0;
    virtual void create(const domain::MonthlyBudget &budget) = 0;
    virtual ~MonthlyBudgetRepository() = default;
};

}

namespace services {

class ProcurementService {
    ports::PurchaseRequestRepository &requests;
    ports::BudgetUpdater &budgets;
    values::BudgetPeriod period;
public:
    ProcurementService(ports::PurchaseRequestRepository &requests,
                       ports::BudgetUpdater &budgets, values::BudgetPeriod period)
        : requests(requests), budgets(budgets), period(period) {}
    void add_line(domain::RequestId request, domain::LineId line, domain::Product product,
                  domain::Quantity quantity) {
        requests.update(request, [line, product = std::move(product),
                                  quantity](domain::PurchaseRequest &aggregate) {
            aggregate.add_line(line, product, quantity);
        });
    }
    void change_quantity(domain::RequestId request, domain::LineId line,
                         domain::Quantity quantity) {
        requests.update(request, [line, quantity](domain::PurchaseRequest &aggregate) {
            aggregate.change_quantity(line, quantity);
        });
    }
    void remove_line(domain::RequestId request, domain::LineId line) {
        requests.update(request, [line](domain::PurchaseRequest &aggregate) {
            aggregate.remove_line(line);
        });
    }
    void submit(domain::RequestId request) {
        auto snapshot = requests.load(request);
        if (!snapshot) throw errors::RequestNotFound(request.value);
        domain::Money total = snapshot->total();
        budgets.update(period, [total](domain::MonthlyBudget &budget) {
            budget.reserve(total);
        });
        try {
            requests.update(request,
                            [](domain::PurchaseRequest &aggregate) { aggregate.submit(); });
        } catch (...) {
            budgets.update(period, [total](domain::MonthlyBudget &budget) {
                budget.release(total);
            });
            throw;
        }
    }
    void cancel(domain::RequestId request, domain::AuthorId actor) {
        auto snapshot = requests.load(request);
        if (!snapshot) throw errors::RequestNotFound(request.value);
        domain::Money total = snapshot->total();
        requests.update(request, [actor](domain::PurchaseRequest &aggregate) {
            aggregate.cancel(actor);
        });
        budgets.update(period, [total](domain::MonthlyBudget &budget) {
            budget.release(total);
        });
    }
};

}

namespace repositories {

using namespace domain;

class InMemoryPurchaseRequestRepository final : public ports::PurchaseRequestRepository {
    std::mutex mutex;
    std::map<std::int64_t, PurchaseRequest> data;
public:
    std::optional<PurchaseRequest> load(RequestId id) override {
        std::lock_guard guard(mutex);
        auto it = data.find(id.value);
        if (it == data.end()) return std::nullopt;
        return it->second;
    }
    void create(const PurchaseRequest &request) override {
        std::lock_guard guard(mutex);
        if (!data.emplace(request.get_id().value, request).second)
            throw errors::RequestAlreadyExists(request.get_id().value);
    }
    void update(RequestId id, const ports::RequestChange &change) override {
        std::lock_guard guard(mutex);
        auto it = data.find(id.value);
        if (it == data.end()) throw errors::RequestNotFound(id.value);
        PurchaseRequest next = it->second;
        change(next);
        it->second = std::move(next);
    }
};

class DirectoryLock {
    std::filesystem::path path;
public:
    DirectoryLock(std::filesystem::path path) : path(std::move(path)) {
        for (int attempt = 0; attempt < 500; ++attempt) {
            std::error_code ec;
            if (std::filesystem::create_directory(this->path, ec)) return;
            if (ec) throw std::filesystem::filesystem_error("создание блокировки", this->path, ec);
            if (!std::filesystem::is_directory(this->path))
                throw errors::LockPathInvalid(this->path);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw errors::LockTimeout(this->path);
    }
    DirectoryLock(const DirectoryLock &) = delete;
    DirectoryLock &operator=(const DirectoryLock &) = delete;
    ~DirectoryLock() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

class FilePurchaseRequestRepository final : public ports::PurchaseRequestRepository {
    std::filesystem::path directory;
    std::filesystem::path path(RequestId id) const {
        return directory / std::format("{}.txt", id.value);
    }
    static PurchaseRequest read(std::istream &in, RequestId id) {
        std::int64_t stored_id, author;
        int state;
        std::size_t count;
        if (!(in >> stored_id >> author >> state >> count) ||
            stored_id != id.value || state < 0 || state > 2 || count > 100000)
            throw errors::InvalidSnapshot(id.value, "заголовок");
        std::vector<RequestLine> rows;
        rows.reserve(count);
        std::string field = "header";
        try {
            for (std::size_t n = 0; n < count; ++n) {
                field = std::format("строка {}", n);
                std::int64_t line_id, kopecks;
                std::string product;
                int quantity;
                if (!(in >> line_id >> product >> quantity >> kopecks))
                    throw errors::InvalidSnapshot(id.value, field);
                rows.emplace_back(LineId{line_id}, Product{product, Money{kopecks}},
                                  Quantity{quantity});
            }
            std::string extra;
            if (in >> extra) throw errors::InvalidSnapshot(id.value, "лишние данные");
            field = "aggregate";
            return PurchaseRequest::restore(id, AuthorId{author},
                                            static_cast<RequestStatus>(state), rows);
        } catch (const errors::DomainError &cause) {
            throw errors::InvalidSnapshot(id.value, field, cause.what());
        }
    }
    static PurchaseRequest read_file(const std::filesystem::path &file, RequestId id) {
        std::ifstream in(file);
        if (!in) throw errors::FileIoError(file, "открытие");
        return read(in, id);
    }
    static void write_file(const std::filesystem::path &file, const PurchaseRequest &request) {
        auto temp = std::filesystem::path(std::format("{}.tmp", file.string()));
        try {
            {
                std::ofstream out(temp, std::ios::trunc);
                if (!out) throw errors::FileIoError(temp, "открытие");
                out << request.get_id().value << ' ' << request.get_author().value << ' '
                    << static_cast<int>(request.get_status()) << ' '
                    << request.get_lines().size() << '\n';
                for (const auto &line : request.get_lines())
                    out << line.get_id().value << ' ' << line.get_product().get_code() << ' '
                        << line.get_quantity().get_count() << ' '
                        << line.get_product().get_price().get_kopecks() << '\n';
                out.flush();
                if (!out) throw errors::FileIoError(temp, "сброс");
                out.close();
                if (!out) throw errors::FileIoError(temp, "закрытие");
            }
            std::filesystem::rename(temp, file);
        } catch (...) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            throw;
        }
    }
public:
    FilePurchaseRequestRepository(std::filesystem::path directory)
        : directory(std::move(directory)) {
        std::filesystem::create_directories(this->directory);
    }
    std::optional<PurchaseRequest> load(RequestId id) override {
        auto file = path(id);
        DirectoryLock lock(std::format("{}.lock", file.string()));
        if (!std::filesystem::exists(file)) return std::nullopt;
        return read_file(file, id);
    }
    void create(const PurchaseRequest &request) override {
        auto file = path(request.get_id());
        DirectoryLock lock(std::format("{}.lock", file.string()));
        if (std::filesystem::exists(file))
            throw errors::RequestAlreadyExists(request.get_id().value);
        write_file(file, request);
    }
    void update(RequestId id, const ports::RequestChange &change) override {
        auto file = path(id);
        DirectoryLock lock(std::format("{}.lock", file.string()));
        if (!std::filesystem::exists(file)) throw errors::RequestNotFound(id.value);
        PurchaseRequest next = read_file(file, id);
        change(next);
        write_file(file, next);
    }
};

class SqlitePurchaseRequestRepository final : public ports::PurchaseRequestRepository {
    SQLite::Database db;
    std::mutex mutex;
    std::optional<PurchaseRequest> read_snapshot(RequestId id) {
        SQLite::Statement head(db, "SELECT author,status FROM requests WHERE id=?;");
        head.bind(1, id.value);
        if (!head.executeStep()) return std::nullopt;
        int raw_status = head.getColumn(1).getInt();
        if (raw_status < 0 || raw_status > 2)
            throw errors::InvalidSnapshot(id.value, "статус");
        std::vector<RequestLine> rows;
        std::string field = "author";
        try {
            AuthorId author{head.getColumn(0).getInt64()};
            SQLite::Statement body(db,
                "SELECT line_id,product,quantity,unit_price "
                "FROM request_lines WHERE request_id=? ORDER BY line_id;");
            body.bind(1, id.value);
            while (body.executeStep()) {
                field = std::format("строка {}", rows.size());
                rows.emplace_back(LineId{body.getColumn(0).getInt64()},
                                  Product{body.getColumn(1).getString(),
                                          Money{body.getColumn(3).getInt64()}},
                                  Quantity{body.getColumn(2).getInt()});
            }
            field = "aggregate";
            return PurchaseRequest::restore(id, author,
                                            static_cast<RequestStatus>(raw_status), rows);
        } catch (const errors::DomainError &cause) {
            throw errors::InvalidSnapshot(id.value, field, cause.what());
        }
    }
    void insert_header(const PurchaseRequest &request) {
        SQLite::Statement add(db, "INSERT INTO requests(id,author,status) VALUES(?,?,?);");
        add.bind(1, request.get_id().value);
        add.bind(2, request.get_author().value);
        add.bind(3, static_cast<int>(request.get_status()));
        add.exec();
    }
    void replace_lines(const PurchaseRequest &request) {
        SQLite::Statement clear(db, "DELETE FROM request_lines WHERE request_id=?;");
        clear.bind(1, request.get_id().value);
        clear.exec();
        SQLite::Statement add(db,
            "INSERT INTO request_lines(request_id,line_id,product,quantity,unit_price) "
            "VALUES(?,?,?,?,?);");
        for (const auto &line : request.get_lines()) {
            add.bind(1, request.get_id().value);
            add.bind(2, line.get_id().value);
            add.bind(3, line.get_product().get_code());
            add.bind(4, line.get_quantity().get_count());
            add.bind(5, line.get_product().get_price().get_kopecks());
            add.exec();
            add.reset();
            add.clearBindings();
        }
    }
    void write_state(const PurchaseRequest &request) {
        SQLite::Statement state(db, "UPDATE requests SET status=? WHERE id=?;");
        state.bind(1, static_cast<int>(request.get_status()));
        state.bind(2, request.get_id().value);
        state.exec();
        replace_lines(request);
    }
public:
    SqlitePurchaseRequestRepository(const std::string &filename)
        : db(filename, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE, 5000) {
        db.exec("PRAGMA foreign_keys=ON;");
        db.exec("CREATE TABLE IF NOT EXISTS requests("
                "id INTEGER PRIMARY KEY, author INTEGER NOT NULL,"
                "status INTEGER NOT NULL);");
        db.exec("CREATE TABLE IF NOT EXISTS request_lines("
                "request_id INTEGER NOT NULL, line_id INTEGER NOT NULL,"
                "product TEXT NOT NULL, quantity INTEGER NOT NULL,"
                "unit_price INTEGER NOT NULL, PRIMARY KEY(request_id,line_id),"
                "FOREIGN KEY(request_id) REFERENCES requests(id));");
    }
    std::optional<PurchaseRequest> load(RequestId id) override {
        std::lock_guard guard(mutex);
        SQLite::Transaction tx(db);
        auto result = read_snapshot(id);
        tx.commit();
        return result;
    }
    void create(const PurchaseRequest &request) override {
        std::lock_guard guard(mutex);
        SQLite::Transaction tx(db, SQLite::TransactionBehavior::IMMEDIATE);
        if (read_snapshot(request.get_id()))
            throw errors::RequestAlreadyExists(request.get_id().value);
        insert_header(request);
        replace_lines(request);
        tx.commit();
    }
    void update(RequestId id, const ports::RequestChange &change) override {
        std::lock_guard guard(mutex);
        SQLite::Transaction tx(db, SQLite::TransactionBehavior::IMMEDIATE);
        auto request = read_snapshot(id);
        if (!request) throw errors::RequestNotFound(id.value);
        change(*request);
        write_state(*request);
        tx.commit();
    }
};

class InMemoryMonthlyBudgetRepository final : public ports::MonthlyBudgetRepository {
    std::mutex mutex;
    std::map<std::string, MonthlyBudget> data;
public:
    std::optional<MonthlyBudget> load(values::BudgetPeriod period) override {
        std::lock_guard guard(mutex);
        auto it = data.find(period.key());
        if (it == data.end()) return std::nullopt;
        return it->second;
    }
    void create(const MonthlyBudget &budget) override {
        std::lock_guard guard(mutex);
        if (!data.emplace(budget.get_period().key(), budget).second)
            throw errors::BudgetAlreadyExists(budget.get_period().key());
    }
    void update(values::BudgetPeriod period, const ports::BudgetChange &change) override {
        std::lock_guard guard(mutex);
        auto it = data.find(period.key());
        if (it == data.end()) throw errors::BudgetNotFound(period.key());
        MonthlyBudget next = it->second;
        change(next);
        it->second = std::move(next);
    }
};

class FileMonthlyBudgetRepository final : public ports::MonthlyBudgetRepository {
    std::filesystem::path directory;
    std::filesystem::path path(values::BudgetPeriod period) const {
        return directory / (period.key() + ".txt");
    }
    static MonthlyBudget read_file(const std::filesystem::path &file,
                                   values::BudgetPeriod period) {
        std::ifstream in(file);
        if (!in) throw errors::FileIoError(file, "открытие");
        std::int64_t limit, consumed;
        if (!(in >> limit >> consumed))
            throw errors::InvalidBudgetSnapshot(period.key(), "разбор снимка");
        try {
            return MonthlyBudget::restore(period, Money{limit}, Money{consumed});
        } catch (const errors::DomainError &cause) {
            throw errors::InvalidBudgetSnapshot(period.key(), cause.what());
        }
    }
    static void write_file(const std::filesystem::path &file, const MonthlyBudget &budget) {
        auto temp = std::filesystem::path(file.string() + ".tmp");
        try {
            {
                std::ofstream out(temp, std::ios::trunc);
                if (!out) throw errors::FileIoError(temp, "открытие");
                out << budget.get_limit().get_kopecks() << ' '
                    << budget.get_consumed().get_kopecks() << '\n';
                out.flush();
                if (!out) throw errors::FileIoError(temp, "сброс");
                out.close();
                if (!out) throw errors::FileIoError(temp, "закрытие");
            }
            std::filesystem::rename(temp, file);
        } catch (...) {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            throw;
        }
    }
public:
    FileMonthlyBudgetRepository(std::filesystem::path directory)
        : directory(std::move(directory)) {
        std::filesystem::create_directories(this->directory);
    }
    std::optional<MonthlyBudget> load(values::BudgetPeriod period) override {
        auto file = path(period);
        DirectoryLock lock(file.string() + ".lock");
        if (!std::filesystem::exists(file)) return std::nullopt;
        return read_file(file, period);
    }
    void create(const MonthlyBudget &budget) override {
        auto file = path(budget.get_period());
        DirectoryLock lock(file.string() + ".lock");
        if (std::filesystem::exists(file))
            throw errors::BudgetAlreadyExists(budget.get_period().key());
        write_file(file, budget);
    }
    void update(values::BudgetPeriod period, const ports::BudgetChange &change) override {
        auto file = path(period);
        DirectoryLock lock(file.string() + ".lock");
        if (!std::filesystem::exists(file)) throw errors::BudgetNotFound(period.key());
        MonthlyBudget next = read_file(file, period);
        change(next);
        write_file(file, next);
    }
};

class SqliteMonthlyBudgetRepository final : public ports::MonthlyBudgetRepository {
    SQLite::Database db;
    std::mutex mutex;
    std::optional<MonthlyBudget> read_snapshot(values::BudgetPeriod period) {
        SQLite::Statement select(db,
            "SELECT limit_kopecks,consumed_kopecks FROM monthly_budget WHERE period=?;");
        select.bind(1, period.key());
        if (!select.executeStep()) return std::nullopt;
        try {
            return MonthlyBudget::restore(period, Money{select.getColumn(0).getInt64()},
                                          Money{select.getColumn(1).getInt64()});
        } catch (const errors::DomainError &cause) {
            throw errors::InvalidBudgetSnapshot(period.key(), cause.what());
        }
    }
public:
    SqliteMonthlyBudgetRepository(const std::string &filename)
        : db(filename, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE, 5000) {
        db.exec("CREATE TABLE IF NOT EXISTS monthly_budget("
                "period TEXT PRIMARY KEY, limit_kopecks INTEGER NOT NULL,"
                "consumed_kopecks INTEGER NOT NULL);");
    }
    std::optional<MonthlyBudget> load(values::BudgetPeriod period) override {
        std::lock_guard guard(mutex);
        SQLite::Transaction tx(db);
        auto result = read_snapshot(period);
        tx.commit();
        return result;
    }
    void create(const MonthlyBudget &budget) override {
        std::lock_guard guard(mutex);
        SQLite::Transaction tx(db, SQLite::TransactionBehavior::IMMEDIATE);
        if (read_snapshot(budget.get_period()))
            throw errors::BudgetAlreadyExists(budget.get_period().key());
        SQLite::Statement add(db,
            "INSERT INTO monthly_budget(period,limit_kopecks,consumed_kopecks) VALUES(?,?,?);");
        add.bind(1, budget.get_period().key());
        add.bind(2, budget.get_limit().get_kopecks());
        add.bind(3, budget.get_consumed().get_kopecks());
        add.exec();
        tx.commit();
    }
    void update(values::BudgetPeriod period, const ports::BudgetChange &change) override {
        std::lock_guard guard(mutex);
        SQLite::Transaction tx(db, SQLite::TransactionBehavior::IMMEDIATE);
        auto budget = read_snapshot(period);
        if (!budget) throw errors::BudgetNotFound(period.key());
        change(*budget);
        SQLite::Statement state(db,
            "UPDATE monthly_budget SET consumed_kopecks=? WHERE period=?;");
        state.bind(1, budget->get_consumed().get_kopecks());
        state.bind(2, period.key());
        state.exec();
        tx.commit();
    }
};

}

}

namespace std {

template <>
struct formatter<procurement::values::RequestId> : formatter<int64_t> {
    constexpr auto format(const procurement::values::RequestId &id,
                          format_context &ctx) const {
        return formatter<int64_t>::format(id.value, ctx);
    }
};
template <>
struct formatter<procurement::values::LineId> : formatter<int64_t> {
    constexpr auto format(const procurement::values::LineId &id, format_context &ctx) const {
        return formatter<int64_t>::format(id.value, ctx);
    }
};
template <>
struct formatter<procurement::values::AuthorId> : formatter<int64_t> {
    constexpr auto format(const procurement::values::AuthorId &id, format_context &ctx) const {
        return formatter<int64_t>::format(id.value, ctx);
    }
};
template <>
struct formatter<procurement::values::Quantity> : formatter<int> {
    constexpr auto format(const procurement::values::Quantity &quantity,
                          format_context &ctx) const {
        return formatter<int>::format(quantity.get_count(), ctx);
    }
};
template <>
struct formatter<procurement::values::Money> : formatter<string_view> {
    constexpr auto format(const procurement::values::Money &money, format_context &ctx) const {
        const auto kopecks = money.get_kopecks();
        return format_to(ctx.out(), "{}.{:02} RUB", kopecks / 100, kopecks % 100);
    }
};
template <>
struct formatter<procurement::values::Product> : formatter<string_view> {
    constexpr auto format(const procurement::values::Product &product,
                          format_context &ctx) const {
        return format_to(ctx.out(), "{} ({})", product.get_code(), product.get_price());
    }
};
template <>
struct formatter<procurement::values::RequestStatus> : formatter<string_view> {
    constexpr auto format(const procurement::values::RequestStatus &status,
                          format_context &ctx) const {
        string_view name = "unknown";
        switch (status) {
        case procurement::values::RequestStatus::draft: name = "draft"; break;
        case procurement::values::RequestStatus::submitted: name = "submitted"; break;
        case procurement::values::RequestStatus::cancelled: name = "cancelled"; break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};
template <>
struct formatter<procurement::values::BudgetPeriod> : formatter<string_view> {
    constexpr auto format(const procurement::values::BudgetPeriod &period,
                          format_context &ctx) const {
        return format_to(ctx.out(), "{:04}-{:02}", period.year, period.month);
    }
};
template <>
struct formatter<procurement::domain::MonthlyBudget> : formatter<string_view> {
    constexpr auto format(const procurement::domain::MonthlyBudget &budget,
                          format_context &ctx) const {
        return format_to(ctx.out(), "{}: потрачено {} из {}", budget.get_period(),
                         budget.get_consumed(), budget.get_limit());
    }
};

}

namespace procurement {

void demo(ports::PurchaseRequestRepository &repository,
          ports::MonthlyBudgetRepository &budgets, const char *name) {
    using values::operator""_RUB;
    const auto period = values::BudgetPeriod{2026, 10};
    if (!budgets.load(period)) budgets.create({period, 1'000'000_RUB});
    services::ProcurementService service(repository, budgets, period);
    auto id = domain::RequestId{42};
    auto author_id = domain::AuthorId{7};
    if (!repository.load(id)) repository.create({id, author_id});
    auto request = repository.load(id);
    if (request->get_status() == domain::RequestStatus::draft &&
        request->get_lines().empty()) {
        service.add_line(id, domain::LineId{1},
                         domain::Product{"PAPER-A4", domain::Money{35000}},
                         domain::Quantity{200});
        service.add_line(id, 2, {"MARKER", 125_RUB}, 6);
        service.add_line(id, 3, {"PEN", 160_RUB}, 10);
        service.change_quantity(id, domain::LineId{2}, domain::Quantity{5});
        service.change_quantity(id, 3, 100);
        service.submit(id);
    }
    request = repository.load(id);
    std::println("{}: lines={}, total={}", name, request->get_lines().size(),
                 request->total());
}

void demo_remove_line(ports::PurchaseRequestRepository &repository,
                      ports::MonthlyBudgetRepository &budgets, const char *name) {
    using values::operator""_RUB;
    const auto period = values::BudgetPeriod{2026, 10};
    if (!budgets.load(period)) budgets.create({period, 1'000'000_RUB});
    services::ProcurementService service(repository, budgets, period);
    const auto request_id = domain::RequestId{77};
    const auto author_id = domain::AuthorId{7};
    if (!repository.load(request_id)) repository.create({request_id, author_id});
    auto request = repository.load(request_id);
    if (request->get_status() == domain::RequestStatus::draft &&
        request->get_lines().empty()) {
        service.add_line(request_id, domain::LineId{1},
                         domain::Product{"PAPER-A4", domain::Money{35000}},
                         domain::Quantity{2});
        service.add_line(request_id, domain::LineId{2},
                         domain::Product{"MARKER", 125_RUB}, domain::Quantity{5});
        service.remove_line(request_id, domain::LineId{1});
        std::println("{}: after remove_line(1) lines={}", name,
                     repository.load(request_id)->get_lines().size());
        try {
            service.remove_line(request_id, domain::LineId{99});
        } catch (const errors::LineNotFound &error) {
            std::println("{}: caught LineNotFound: {}", name, error.what());
        }
        service.submit(request_id);
        try {
            service.remove_line(request_id, domain::LineId{2});
        } catch (const errors::RequestNotDraft &error) {
            std::println("{}: caught RequestNotDraft: {}", name, error.what());
        }
    }
    const auto empty_id = domain::RequestId{78};
    if (!repository.load(empty_id)) repository.create({empty_id, author_id});
    auto empty_request = repository.load(empty_id);
    if (empty_request->get_status() == domain::RequestStatus::draft &&
        empty_request->get_lines().empty()) {
        service.add_line(empty_id, domain::LineId{1},
                         domain::Product{"PEN", 160_RUB}, domain::Quantity{10});
        service.remove_line(empty_id, domain::LineId{1});
        empty_request = repository.load(empty_id);
        std::println("{}: draft {} still exists, lines={}", name, empty_id.value,
                     empty_request->get_lines().size());
        try {
            service.submit(empty_id);
        } catch (const errors::EmptyRequest &error) {
            std::println("{}: caught EmptyRequest: {}", name, error.what());
        }
    }
}

void demo_monthly_budget(ports::PurchaseRequestRepository &repository,
                         ports::MonthlyBudgetRepository &budgets, const char *name) {
    using values::operator""_RUB;
    const auto period = values::BudgetPeriod{2026, 11};
    if (!budgets.load(period)) budgets.create({period, 1000_RUB});
    services::ProcurementService service(repository, budgets, period);
    const auto author_id = domain::AuthorId{7};
    const auto paper_id = domain::RequestId{91};
    const auto marker_id = domain::RequestId{92};
    const auto chair_id = domain::RequestId{93};
    const auto desk_id = domain::RequestId{94};
    if (!repository.load(paper_id)) repository.create({paper_id, author_id});
    auto paper = repository.load(paper_id);
    if (paper->get_status() == domain::RequestStatus::draft &&
        paper->get_lines().empty()) {
        service.add_line(paper_id, domain::LineId{1},
                         domain::Product{"PAPER-A4", 350_RUB}, domain::Quantity{2});
        service.submit(paper_id);
        std::println("{}: budget after submit 91: {}", name, budgets.load(period).value());
        repository.create({marker_id, author_id});
        service.add_line(marker_id, domain::LineId{1},
                         domain::Product{"MARKER", 125_RUB}, domain::Quantity{4});
        try {
            service.submit(marker_id);
        } catch (const errors::BudgetExceeded &error) {
            std::println("{}: caught BudgetExceeded: {}", name, error.what());
        }
        service.add_line(marker_id, domain::LineId{2},
                         domain::Product{"PEN", 160_RUB}, domain::Quantity{1});
        repository.create({chair_id, author_id});
        service.add_line(chair_id, domain::LineId{1},
                         domain::Product{"CHAIR", 300_RUB}, domain::Quantity{1});
        service.cancel(paper_id, author_id);
        std::println("{}: budget after cancel 91: {}", name, budgets.load(period).value());
        service.submit(marker_id);
        service.submit(chair_id);
        repository.create({desk_id, author_id});
        service.add_line(desk_id, domain::LineId{1},
                         domain::Product{"DESK", 500_RUB}, domain::Quantity{1});
        try {
            service.submit(desk_id);
        } catch (const errors::BudgetExceeded &error) {
            std::println("{}: caught BudgetExceeded: {}", name, error.what());
        }
        auto desk = repository.load(desk_id);
        std::println("{}: draft 94: status={}, lines={}", name, desk->get_status(),
                     desk->get_lines().size());
    }
    std::println("{}: final budget: {}", name, budgets.load(period).value());
}

}

int main() {
    using namespace procurement;
    try {
        repositories::InMemoryPurchaseRequestRepository memory;
        repositories::InMemoryMonthlyBudgetRepository memory_budgets;
        repositories::FilePurchaseRequestRepository files{"./requests"};
        repositories::FileMonthlyBudgetRepository file_budgets{"./budgets"};
        repositories::SqlitePurchaseRequestRepository sqlite{"./requests.sqlite"};
        repositories::SqliteMonthlyBudgetRepository sqlite_budgets{"./budgets.sqlite"};
        demo(memory, memory_budgets, "memory");
        demo(files, file_budgets, "file");
        demo(sqlite, sqlite_budgets, "sqlite");
        demo_remove_line(memory, memory_budgets, "memory");
        demo_remove_line(files, file_budgets, "file");
        demo_remove_line(sqlite, sqlite_budgets, "sqlite");
        demo_monthly_budget(memory, memory_budgets, "memory");
        demo_monthly_budget(files, file_budgets, "file");
        demo_monthly_budget(sqlite, sqlite_budgets, "sqlite");
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::println(stderr, "{}", error.what());
        return EXIT_FAILURE;
    }
}