/** Helper classes for defining and executing prepared statements.
 *
 * See the connection_base hierarchy for more about prepared statements.
 *
 * Copyright (c) 2006-2017, Jeroen T. Vermeulen.
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 */
#ifndef PQXX_H_PREPARED_STATEMENT
#define PQXX_H_PREPARED_STATEMENT

#include "pqxx/compiler-public.hxx"
#include "pqxx/compiler-internal-pre.hxx"

#include "pqxx/internal/statement_parameters.hxx"

#include "pqxx/types"


namespace pqxx
{
/// Dedicated namespace for helper types related to prepared statements
namespace prepare
{
/** \defgroup prepared Prepared statements
 *
 * Prepared statements are SQL queries that you define once and then invoke
 * as many times as you like, typically with varying parameters.  It's basically
 * a function that you can define ad hoc.
 *
 * If you have an SQL statement that you're going to execute many times in
 * quick succession, it may be more efficient to prepare it once and reuse it.
 * This saves the database backend the effort of parsing complex SQL and
 * figuring out an efficient execution plan.  Another nice side effect is that
 * you don't need to worry about escaping parameters.
 *
 * You create a prepared statement by preparing it on the connection
 * (using the @c pqxx::connection_base::prepare functions), passing an
 * identifier and its SQL text.  The identifier is the name by which the
 * prepared statement will be known; it should consist of ASCII letters,
 * digits, and underscores only, and start with an ASCII letter.  The name is
 * case-sensitive.
 *
 * @code
 * void prepare_my_statement(pqxx::connection_base &c)
 * {
 *   c.prepare("my_statement", "SELECT * FROM Employee WHERE name = 'Xavier'");
 * }
 * @endcode
 *
 * Once you've done this, you'll be able to call @c my_statement from any
 * transaction you execute on the same connection.  For this, use the
 * @c pqxx::transaction_base::exec_prepared functions.
 *
 * @code
 * pqxx::result execute_my_statement(pqxx::transaction_base &t)
 * {
 *   return t.exec_prepared("my_statement");
 * }
 * @endcode
 *
 * Did I mention that prepared statements can have parameters?  The query text
 * can contain @c $1, @c $2 etc. as placeholders for parameter values that you
 * will provide when you invoke the prepared satement.
 *
 * @code
 * void prepare_find(pqxx::connection_base &c)
 * {
 *   // Prepare a statement called "find" that looks for employees with a given
 *   // name (parameter 1) whose salary exceeds a given number (parameter 2).
 *   c.prepare(
 *   	"find",
 *   	"SELECT * FROM Employee WHERE name = $1 AND salary > $2");
 * }
 * @endcode
 *
 * This example looks up the prepared statement "find," passes @c name and
 * @c min_salary as parameters, and invokes the statement with those values:
 *
 * @code
 * pqxx::result execute_find(
 *   pqxx::transaction_base &t, std::string name, int min_salary)
 * {
 *   return t.exec_prepared("find", name, min_salary);
 * }
 * @endcode
 *
 * A special case is the nameless prepared statement.  You may prepare a
 * statement without a name.  The unnamed statement can be redefined at any
 * time, without un-preparing it first.
 *
 * Never try to prepare, execute, or unprepare a prepared statement
 * manually using direct SQL queries.  Always use the functions provided by
 * libpqxx.
 *
 * Prepared statements are not necessarily defined on the backend right away.
 * It's usually done lazily.  This means that you can prepare statements before
 * the connection is fully established, and that it's relatively cheap to
 * pre-prepare lots of statements that you may or may not not use during the
 * session.  On the other hand, it also means that errors in a prepared
 * statement may not show up until you first try to invoke it.  Such an error
 * may then break the transaction it occurs in.
 *
 * A performance note: There are cases where prepared statements are actually
 * slower than plain SQL.  Sometimes the backend can produce a better execution
 * plan when it knows the parameter values.  For example, say you've got a web
 * application and you're querying for users with status "inactive" who have
 * email addresses in a given domain name X.  If X is a very popular provider,
 * the best way for the database engine to plan the query may be to list the
 * inactive users first and then filter for the email addresses you're looking
 * for.  But in other cases, it may be much faster to find matching email
 * addresses first and then see which of their owners are "inactive."  A
 * prepared statement must be planned to fit either case, but a direct query
 * will be optimised based on table statistics, partial indexes, etc.
 *
 * @warning Beware of "nul" bytes.  Any string you pass as a parameter will
 * end at the first char with value zero.  If you pass a @c std::string that
 * contains a zero byte, the last byte in the value will be the one just
 * before the zero.  If you need a zero byte, consider using
 * pqxx::binarystring and/or SQL's @c bytea type.
 */

/// Helper class for passing parameters to, and executing, prepared statements
/** @deprecated As of 6.0, use @c transaction_base::exec_prepared and friends.
 */
class PQXX_LIBEXPORT invocation : internal::statement_parameters
{
public:
  invocation(transaction_base &, const std::string &statement);
  invocation &operator=(const invocation &) =delete;

  /// Execute!
  result exec() const;

  /// Has a statement of this name been defined?
  bool exists() const;

  /// Pass null parameter.
  invocation &operator()() { add_param(); return *this; }

  /// Pass parameter value.
  /**
   * @param v parameter value; will be represented as a string internally.
   */
  template<typename T> invocation &operator()(const T &v)
	{ add_param(v, true); return *this; }

  /// Pass binary parameter value for a BYTEA field.
  /**
   * @param v binary string; will be passed on directly in binary form.
   */
  invocation &operator()(const binarystring &v)
	{ add_binary_param(v, true); return *this; }

  /// Pass parameter value.
  /**
   * @param v parameter value (will be represented as a string internally).
   * @param nonnull replaces value with null if set to false.
   */
  template<typename T> invocation &operator()(const T &v, bool nonnull)
	{ add_param(v, nonnull); return *this; }

  /// Pass binary parameter value for a BYTEA field.
  /**
   * @param v binary string; will be passed on directly in binary form.
   * @param nonnull determines whether to pass a real value, or nullptr.
   */
  invocation &operator()(const binarystring &v, bool nonnull)
	{ add_binary_param(v, nonnull); return *this; }

  /// Pass C-style parameter string, or null if pointer is null.
  /**
   * This version is for passing C-style strings; it's a template, so any
   * pointer type that @c to_string accepts will do.
   *
   * @param v parameter value (will be represented as a C++ string internally)
   * @param nonnull replaces value with null if set to @c false
   */
  template<typename T> invocation &operator()(T *v, bool nonnull=true)
	{ add_param(v, nonnull); return *this; }

  /// Pass C-style string parameter, or null if pointer is null.
  /** This duplicates the pointer-to-template-argument-type version of the
   * operator, but helps compilers with less advanced template implementations
   * disambiguate calls where C-style strings are passed.
   */
  invocation &operator()(const char *v, bool nonnull=true)
	{ add_param(v, nonnull); return *this; }

private:
  transaction_base &m_home;
  const std::string m_statement;
  std::vector<std::string> m_values;
  std::vector<bool> m_nonnull;

  invocation &setparam(const std::string &, bool nonnull);
};


namespace internal
{
/// Internal representation of a prepared statement definition.
struct PQXX_LIBEXPORT prepared_def
{
  /// Text of prepared query.
  std::string definition;
  /// Has this prepared statement been prepared in the current session?
  bool registered = false;

  prepared_def() =default;
  explicit prepared_def(const std::string &);
};

} // namespace pqxx::prepare::internal
} // namespace pqxx::prepare
} // namespace pqxx

#include "pqxx/compiler-internal-post.hxx"

#endif
