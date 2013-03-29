package org.ejdb.driver;

import org.ejdb.bson.BSONObject;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Query/BSON builder is used to create EJDB queries.
 * EJDBQueryBuilder can be used to construct BSON objects as well as queries.
 *
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public class EJDBQueryBuilder {
    private EJDBQueryBuilder parent;

    private BSONObject query;
    private List<BSONObject> queryOrs;
    private BSONObject hints;

    public EJDBQueryBuilder() {
        query = new BSONQueryObject();
        queryOrs = new ArrayList<BSONObject>();
        hints = new BSONQueryObject();
    }

    public EJDBQueryBuilder(BSONObject query, List<BSONObject> queryOrs, BSONObject hints) {
        this.query = query;
        this.queryOrs = queryOrs != null ? queryOrs : new ArrayList<BSONObject>();
        this.hints = hints != null ? hints : new BSONQueryObject();
    }

    protected EJDBQueryBuilder(EJDBQueryBuilder parent, BSONObject query) {
        this.parent = parent;
        this.query = query;
    }

    /**
     * @return main BSON query object
     */
    public BSONObject getMainQuery() {
        return query;
    }

    /**
     * @return BSON objects for additional OR queries
     */
    public BSONObject[] getOrQueries() {
        if (queryOrs == null) {
            return new BSONObject[0];
        }

        int i = -1;
        BSONObject[] ors = new BSONObject[queryOrs.size()];
        for (BSONObject qor : queryOrs) {
            ors[++i] = qor;
        }

        return ors;
    }

    /**
     * @return BSON hints object
     */
    public BSONObject getQueryHints() {
        return hints;
    }

    /**
     * Adds query restrintions in main query object.
     *
     * @param field   field path
     * @param value   field value
     * @param replace if <code>true</code> all other restrictions will be replaces, otherwise trying to add restrictions for field
     */
    protected EJDBQueryBuilder addOperation(String field, Object value, boolean replace) {
        if (!query.containsField(field) || replace || !(value instanceof BSONObject)) {
            query.put(field, value);
        } else {
            Object cvalue = query.get(field);
            if (cvalue instanceof BSONObject) {
                ((BSONObject) cvalue).putAll((BSONObject) value);
            } else {
                query.put(field, value);
            }
        }
        return this;
    }

    /**
     * Checks hints section allowed.
     *
     * @throws EJDBException if hints section if not allowed for current EJDBQueryBuilder object
     */
    protected void checkHintsAvailable() throws EJDBException {
        if (hints == null) {
            throw new EJDBException("hints section not available for subquery such as or-query or element match query");
        }
    }

    /**
     * Adds pair name->value to hints BSON object.
     *
     * @throws EJDBException if hints section if not allowed for current EJDBQueryBuilder object
     */
    protected EJDBQueryBuilder addHint(String name, Object value) throws EJDBException {
        checkHintsAvailable();
        hints.put(name, value);
        return this;
    }

    /**
     * Adds field equality restriction.
     * <p/>
     * <code>query.field(field, value); // -> {field : value}</code>
     */
    public EJDBQueryBuilder field(String field, Object value) {
        return this.field(field).eq(value);
    }

    /**
     * Adds contraint for field
     */
    public Constraint field(String field) {
        return new Constraint(field, false);
    }

    /**
     * Element match construction.
     * - $elemMatch The $elemMatch operator matches more than one component within an array element.
     * - { array: { $elemMatch: { value1 : 1, value2 : { $gt: 1 } } } }
     * <p/>
     * Restriction: only one $elemMatch allowed in context of one array field.
     */
    public EJDBQueryBuilder elementMatch(String field) {
        BSONQueryObject emqbson = new BSONQueryObject();
        query.put(field, new BSONQueryObject("$elemMatch", emqbson));
        return new EJDBQueryBuilder(null, emqbson);
    }

    /**
     * Add <code>OR</code> joined query restrictions.
     *
     * @throws EJDBException if or section if not allowed for current EJDBQueryBuilder object (in ElementMatch-query, for example)
     */
    public EJDBQueryBuilder or() throws EJDBException {
        if (parent != null) {
            return parent.or();
        }

        if (queryOrs == null) {
            throw new EJDBException("or section not available for subquery such as element match query");
        }

        BSONQueryObject qor = new BSONQueryObject();
        queryOrs.add(qor);
        return new EJDBQueryBuilder(this, qor);
    }

    /**
     * Set specified fiels to value
     * <p/>
     * <code>query.set(field1, value1).set(field2, value2); // -> { ..., $set : {field1 : value1, field2 : value2}}</code>
     */
    public EJDBQueryBuilder set(String field, Object value) {
        return new Constraint(field, new Constraint("$set", false)).addOperation(value);
    }

    /**
     * Atomic upsert.
     * If matching records are found it will be <code>$set</code> operation, otherwise new record will be inserted with field specified by value.
     * <p/>
     * <code>query.field(field, value).upsert(field, value); // -> {field : value, $upsert : {field : value}}</code>
     */
    public EJDBQueryBuilder upsert(String field, Object value) {
        return new Constraint(field, new Constraint("$upsert", false)).addOperation(value);
    }

    /**
     * Increment specified field. Only number types are supported.
     * <p/>
     * <code>query.int(field1, value1).int(field2, value2); // -> { ..., $int : {field1 : value1, field2 : value2}}</code>
     */
    public EJDBQueryBuilder inc(String field, Number inc) {
        return new Constraint(field, new Constraint("$inc", false)).addOperation(inc);
    }

    /**
     * In-place record removal operation.
     * <p/>
     * Example:
     * Next update query removes all records with name eq 'andy':
     * <code>query.field("name", "andy").dropAll()</code>
     */
    public EJDBQueryBuilder dropAll() {
        return new Constraint("$dropAll").addOperation(true);
    }

    /**
     * Atomically adds value to the array field only if value not in the array already. If containing array is missing it will be created.
     */
    public EJDBQueryBuilder addToSet(String field, Object value) {
        return new Constraint(field, new Constraint("$addToSet", false)).addOperation(value);
    }

    /**
     * Atomically performs <code>set union</code> with values in val for specified array field.
     */
    public EJDBQueryBuilder addToSetAll(String field, Object... values) {
        return new Constraint(field, new Constraint("$addToSetAll", false)).addOperation((Object[]) values);
    }

    /**
     * Atomically performs <code>set union</code> with values in val for specified array field.
     */
    public EJDBQueryBuilder addToSetAll(String field, Collection<Object> values) {
        Object[] objects = new Object[values.size()];
        values.toArray(objects);
        return new Constraint(field, new Constraint("$addToSetAll", false)).addOperation(objects);
    }

    /**
     * Atomically removes all occurrences of value from field, if field is an array.
     */
    public EJDBQueryBuilder pull(String field, Object value) {
        return new Constraint(field, new Constraint("$pull", false)).addOperation(value);
    }

    /**
     * Atomically performs <code>set substraction</code> of values for specified array field.
     */
    public EJDBQueryBuilder pullAll(String field, Object... values) {
        return new Constraint(field, new Constraint("$pullAll", false)).addOperation((Object[]) values);
    }

    /**
     * Atomically performs <code>set substraction</code> of values for specified array field.
     */
    public EJDBQueryBuilder pullAll(String field, Collection<Object> values) {
        Object[] objects = new Object[values.size()];
        values.toArray(objects);
        return new Constraint(field, new Constraint("$pullAll", false)).addOperation(objects);
    }

    /**
     * Make {@se http://github.com/Softmotions/ejdb/wiki/Collection-joins collection join} for select queries.
     */
    public EJDBQueryBuilder join(String fpath, String collname) {
        return new Constraint("$join", new Constraint(fpath, new Constraint("$do", false))).addOperation(collname);
    }

    /**
     * Sets max number of records in the result set.
     */
    public EJDBQueryBuilder setMaxResults(int maxResults) {
        return addHint("$max", maxResults);
    }

    /**
     * Sets number of skipped records in the result set.
     */
    public EJDBQueryBuilder setOffset(int offset) {
        return addHint("$skip", offset);
    }

    /**
     * Sets fields to be included or exluded in resulting objects.
     * If field presented in <code>$orderby</code> clause it will be forced to include in resulting records.
     */
    public EJDBQueryBuilder setFieldIncluded(String field, boolean incldue) {
        checkHintsAvailable();

        BSONObject fields = hints.containsField("$fields") && (hints.get("$fields") instanceof BSONObject) ? (BSONObject) hints.get("$fields") : new BSONQueryObject();
        fields.put(field, incldue ? 1 : -1);
        hints.put("$fields", fields);

        return this;
    }

    /**
     * Sets fields to be included in resulting objects.
     * If field presented in <code>$orderby</code> clause it will be forced to include in resulting records.
     */
    public EJDBQueryBuilder includeField(String field) {
        return setFieldIncluded(field, true);
    }

    /**
     * Sets fields to be excluded from resulting objects.
     * If field presented in <code>$orderby</code> clause it will be forced to include in resulting records.
     */
    public EJDBQueryBuilder excludeField(String field) {
        return setFieldIncluded(field, false);
    }

    /**
     * Resturs return sorting rules control object
     */
    public OrderBy orderBy() {
        checkHintsAvailable();

        if (hints.containsField("$orderby") && hints.get("$orderby") != null && (hints.get("$orderby") instanceof BSONObject)) {
            return new OrderBy((BSONObject) hints.get("$orderby"));
        } else {
            return new OrderBy((BSONObject) hints.put("$orderby", new BSONQueryObject()));
        }
    }

    /**
     * Find constraint for specified field
     */
    public class Constraint {
        private String field;
        private boolean replace;
        private Constraint parent;

        protected Constraint(String field) {
            this(field, true);
        }

        protected Constraint(String field, boolean replace) {
            this.field = field;
            this.replace = replace;
        }

        protected Constraint(String field, Constraint parent) {
            this.field = field;
            this.parent = parent;
            this.replace = true;
        }

        /**
         * Add curently restrictons tree to query
         */
        protected EJDBQueryBuilder addOperation(Object value) {
            if (parent != null) {
                return parent.addOperation(new BSONQueryObject(field, value));
            } else {
                return EJDBQueryBuilder.this.addOperation(field, value, replace);
            }
        }

        /**
         * Add <code>$not</code> negatiation contraint
         * <p/>
         * Example:
         * <code>query.field(field).not().eq(value); // {field : { $not : value }}</code>
         * <code>query.field(field).not().bt(start, end); // {field : { $not : {$bt : [start, end]}}}</code>
         */
        public Constraint not() {
            return new Constraint("$not", this);
        }

        /**
         * Field equality restriction.
         * All usage samples represent same thing: {"field" : value}
         * <p/>
         * Example:
         * <code>query.field(field, value); // -> {field : value}</code>
         * <code>query.field(field).eq(value); // -> {field : value}</code>
         */
        public EJDBQueryBuilder eq(Object value) {
            return this.addOperation(value);
        }

        /**
         * Greater than or equal value (field_value >= value)
         */
        public EJDBQueryBuilder gte(Number value) {
            return new Constraint("$gte", this).addOperation(value);
        }

        /**
         * Greater than value (field_value > value)
         */
        public EJDBQueryBuilder gt(Number value) {
            return new Constraint("$gt", this).addOperation(value);
        }

        /**
         * Lesser then or equal value (field_value <= value)
         */
        public EJDBQueryBuilder lte(Number value) {
            return new Constraint("$lte", this).addOperation(value);
        }

        /**
         * Lesser then value (field_value < value)
         */
        public EJDBQueryBuilder lt(Number value) {
            return new Constraint("$lt", this).addOperation(value);
        }

        /**
         * Between number (start <= field_value <= end)
         */
        public EJDBQueryBuilder bt(Number start, Number end) {
            return new Constraint("$bt", this).addOperation(new Number[]{start, end});
        }

        /**
         * Field value matched <b>any</b> value of specified in values.
         */
        public EJDBQueryBuilder in(Object... values) {
            return new Constraint("$in", this).addOperation((Object[]) values);
        }

        /**
         * Field value matched <b>any</b> value of specified in values.
         */
        public EJDBQueryBuilder in(Collection<Object> values) {
            Object[] objects = new Object[values.size()];
            values.toArray(objects);
            return new Constraint("$in", this).addOperation(objects);
        }

        /**
         * Negation of {@link Constraint#in(Object...)}
         */
        public EJDBQueryBuilder notIn(Object... values) {
            return new Constraint("$nin", this).addOperation((Object[]) values);
        }

        /**
         * Negation of {@link Constraint#in(java.util.Collection)}
         */
        public EJDBQueryBuilder notIn(Collection<Object> values) {
            Object[] objects = new Object[values.size()];
            values.toArray(objects);
            return new Constraint("$nin", this).addOperation(objects);
        }

        /**
         * Strins starts with prefix
         */
        public EJDBQueryBuilder begin(String value) {
            return new Constraint("$begin", this).addOperation(value);
        }

        /**
         * String tokens (or string array vals) matches <b>all</b> tokens in specified array.
         */
        public EJDBQueryBuilder strAnd(String... values) {
            return new Constraint("$strand", this).addOperation((String[]) values);
        }

        /**
         * String tokens (or string array vals) matches <b>all</b> tokens in specified collection.
         */
        public EJDBQueryBuilder strAnd(Collection<String> values) {
            String[] strs = new String[values.size()];
            values.toArray(strs);
            return new Constraint("$strand", this).addOperation(strs);
        }

        /**
         * String tokens (or string array vals) matches <b>any</b> tokens in specified array.
         */
        public EJDBQueryBuilder strOr(String... values) {
            return new Constraint("$stror", this).addOperation((String[]) values);
        }

        /**
         * String tokens (or string array vals) matches <b>any</b> tokens in specified collection.
         */
        public EJDBQueryBuilder strOr(Collection<String> values) {
            String[] strs = new String[values.size()];
            values.toArray(strs);
            return new Constraint("$stror", this).addOperation(strs);
        }

        /**
         * Field existence matching {@link Constraint#exists(boolean = true)}
         */
        public EJDBQueryBuilder exists() {
            return this.exists(true);
        }

        /**
         * Field existence matching
         */
        public EJDBQueryBuilder exists(boolean exists) {
            return new Constraint("$exists", this).addOperation(exists);
        }

        /**
         * Case insensitive string matching
         * <p/>
         * Example:
         * <code>query.field(field).icase().eq(value); // -> {field : {$icase : value}}</code>
         * <code>query.field(field).icase().in(value1, value2); // -> {field : {$icase : {$in : [value1, value2]}}}</code>
         */
        public Constraint icase() {
            return new Constraint("$icase", this);
        }
    }

    /**
     * Sorting rules for query results
     */
    public class OrderBy {
        private BSONObject orderBy;

        protected OrderBy(BSONObject orderBy) {
            this.orderBy = orderBy;
        }

        /**
         * Add ascending sorting order for field
         *
         * @param field BSON field path
         */
        public OrderBy asc(String field) {
            return add(field, true);
        }

        /**
         * Add descinding sorting order for field
         *
         * @param field BSON field path
         */
        public OrderBy desc(String field) {
            return add(field, false);
        }

        /**
         * Add sorting order for field
         *
         * @param field BSON field path
         * @param asc   if <code>true</code> ascendong sorting order, otherwise - descinding
         */
        public OrderBy add(String field, boolean asc) {
            orderBy.put(field, asc ? 1 : -1);
            return this;
        }

        /**
         * Clear all current sorting rules
         */
        public OrderBy clear() {
            orderBy.clear();
            return this;
        }
    }

}
