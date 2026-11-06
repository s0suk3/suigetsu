package queue

import (
	"database/sql"
	_ "github.com/mattn/go-sqlite3"
)

type Queue struct {
	db *sql.DB
}

func NewQueue(dbPath string) (*Queue, error) {
	db, err := sql.Open("sqlite3", dbPath)
	if err != nil {
		return nil, err
	}
	_, err = db.Exec(`
		CREATE TABLE IF NOT EXISTS tasks (
			id INTEGER PRIMARY KEY,
			implant_id TEXT,
			task TEXT,
			result TEXT,
			state TEXT DEFAULT 'pending'
		)`)
	if err != nil {
		return nil, err
	}
	_, err = db.Exec(`
		CREATE TABLE IF NOT EXISTS implants (
			id TEXT PRIMARY KEY,
			first_seen DATETIME,
			last_seen DATETIME,
			info TEXT
		)`)
	if err != nil {
		return nil, err
	}
	return &Queue{db: db}, nil
}

func (q *Queue) AddTask(implantID string, task string) error {
	_, err := q.db.Exec("INSERT INTO tasks (implant_id, task) VALUES (?, ?)", implantID, task)
	return err
}

func (q *Queue) GetNextTask(implantID string) (string, error) {
	row := q.db.QueryRow("SELECT id, task FROM tasks WHERE implant_id = ? AND state = 'pending' ORDER BY id LIMIT 1", implantID)
	var id int64
	var task string
	err := row.Scan(&id, &task)
	if err != nil {
		return "", err
	}
	_, err = q.db.Exec("UPDATE tasks SET state = 'delivered' WHERE id = ?", id)
	if err != nil {
		return "", err
	}
	return task, nil
}

func (q *Queue) CompleteTask(implantID string, result string) error {
	_, err := q.db.Exec(`
		UPDATE tasks 
		SET state = 'completed', result = ? 
		WHERE id = (
			SELECT id FROM tasks 
			WHERE implant_id = ? AND state = 'delivered' 
			ORDER BY id LIMIT 1
		)`, result, implantID)
	return err
}

func (q *Queue) GetResults(implantID string) ([]string, error) {
	rows, err := q.db.Query("SELECT result FROM tasks WHERE implant_id = ? AND state = 'completed' AND result IS NOT NULL", implantID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var results []string
	for rows.Next() {
		var result string
		if err := rows.Scan(&result); err == nil {
			results = append(results, result)
		}
	}
	return results, nil
}

func (q *Queue) GetHistory(implantID string) ([]string, error) {
	rows, err := q.db.Query("SELECT task, state FROM tasks WHERE implant_id = ? ORDER BY id", implantID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var history []string
	for rows.Next() {
		var task, state string
		if err := rows.Scan(&task, &state); err == nil {
			history = append(history, task+" ["+state+"]")
		}
	}
	return history, nil
}

type ImplantInfo struct {
	ID        string
	FirstSeen string
	LastSeen  string
	Info      string
}

func (q *Queue) ListImplants() ([]ImplantInfo, error) {
	rows, err := q.db.Query("SELECT id, first_seen, last_seen, info FROM implants ORDER BY last_seen DESC")
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var list []ImplantInfo
	for rows.Next() {
		var imp ImplantInfo
		if err := rows.Scan(&imp.ID, &imp.FirstSeen, &imp.LastSeen, &imp.Info); err == nil {
			list = append(list, imp)
		}
	}
	return list, nil
}

func (q *Queue) RegisterImplant(implantID string, info string) error {
	_, err := q.db.Exec("INSERT OR IGNORE INTO implants (id, first_seen, last_seen, info) VALUES (?, datetime('now'), datetime('now'), ?)", implantID, info)
	return err
}

func (q *Queue) UpdateLastSeen(implantID string) error {
	_, err := q.db.Exec("UPDATE implants SET last_seen = datetime('now') WHERE id = ?", implantID)
	return err
}