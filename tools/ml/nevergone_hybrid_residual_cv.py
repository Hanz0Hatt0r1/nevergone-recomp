#!/usr/bin/env python3
"""Evaluate a TF-IDF + tiny neural residual classifier with family-grouped CV.

The linear path preserves the strong sparse portable-P-code signal. A much smaller
neural encoder is trained only as a residual correction on top of fixed linear
logits. Inner family-grouped validation is allowed to select epoch 0, so a fold
falls back to the linear baseline when the neural residual does not help.
"""
from __future__ import annotations

import argparse
import json
import math
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

from nevergone_pcode_features import portable_pcode
from nevergone_tiny_encoder_features import hashed_feature_counts


def _require_dependencies():
    try:
        import numpy as np
        from sklearn.feature_extraction.text import TfidfVectorizer
        from sklearn.linear_model import LogisticRegression
        from sklearn.metrics import (
            balanced_accuracy_score,
            classification_report,
            confusion_matrix,
            f1_score,
        )
        from sklearn.model_selection import StratifiedGroupKFold
    except ImportError as exc:
        raise SystemExit(
            "This evaluator requires numpy and scikit-learn. Install with: "
            "python -m pip install -U numpy scikit-learn"
        ) from exc
    return (
        np,
        TfidfVectorizer,
        LogisticRegression,
        balanced_accuracy_score,
        classification_report,
        confusion_matrix,
        f1_score,
        StratifiedGroupKFold,
    )


def _read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            try:
                row = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_no}: invalid JSON: {exc}") from exc
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def _load_family_map(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for row in _read_jsonl(path):
        record_id = row.get("id")
        family_id = row.get("family_id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("family row missing id")
        if not isinstance(family_id, str) or not family_id:
            raise ValueError(f"family row {record_id} missing family_id")
        if record_id in result and result[record_id] != family_id:
            raise ValueError(f"conflicting family IDs for {record_id}")
        result[record_id] = family_id
    return result


def _load_dataset(
    dataset_dir: Path,
    family_path: Path,
    task: str,
    classes: list[str],
) -> list[dict[str, Any]]:
    family_map = _load_family_map(family_path)
    allowed = set(classes)
    rows: list[dict[str, Any]] = []
    seen: set[str] = set()
    for split in ("train", "validation", "test"):
        path = dataset_dir / f"{split}.jsonl"
        for row in _read_jsonl(path):
            record_id = row.get("id")
            text = row.get("text")
            labels = row.get("labels")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}: row missing id")
            if record_id in seen:
                raise ValueError(f"duplicate dataset id: {record_id}")
            seen.add(record_id)
            if not isinstance(text, str) or not text:
                raise ValueError(f"dataset row {record_id} missing text")
            if not isinstance(labels, dict):
                continue
            label = labels.get(task)
            if label not in allowed:
                continue
            family_id = family_map.get(record_id)
            if family_id is None:
                raise ValueError(f"dataset id missing from family map: {record_id}")
            rows.append(
                {
                    "id": record_id,
                    "text": portable_pcode(text),
                    "label": label,
                    "family_id": family_id,
                }
            )
    return rows


def residual_parameter_count(*, buckets: int, embedding_dim: int, hidden_dim: int, classes: int) -> int:
    return (
        buckets * embedding_dim
        + embedding_dim * hidden_dim
        + hidden_dim
        + hidden_dim * classes
        + classes
    )


class ResidualEncoder:
    def __init__(
        self,
        np,
        *,
        buckets: int,
        embedding_dim: int,
        hidden_dim: int,
        classes: int,
        seed: int,
    ) -> None:
        self.np = np
        self.rng = np.random.default_rng(seed)
        e_scale = 0.25 / math.sqrt(max(1, embedding_dim))
        h_scale = math.sqrt(2.0 / max(1, embedding_dim + hidden_dim))
        self.embedding = self.rng.normal(
            0.0, e_scale, size=(buckets, embedding_dim)
        ).astype(np.float32)
        self.w1 = self.rng.normal(
            0.0, h_scale, size=(embedding_dim, hidden_dim)
        ).astype(np.float32)
        self.b1 = np.zeros(hidden_dim, dtype=np.float32)
        self.w2 = np.zeros((hidden_dim, classes), dtype=np.float32)
        self.b2 = np.zeros(classes, dtype=np.float32)

    def _encode(self, feature_row):
        np = self.np
        ids = np.asarray([item[0] for item in feature_row], dtype=np.int64)
        counts = np.asarray([item[1] for item in feature_row], dtype=np.float32)
        weights = np.log1p(counts)
        norm = float(np.linalg.norm(weights))
        if norm > 0.0:
            weights /= norm
        z = (self.embedding[ids] * weights[:, None]).sum(axis=0)
        h = np.tanh(z @ self.w1 + self.b1)
        residual = h @ self.w2 + self.b2
        return ids, weights, z, h, residual

    def predict(self, features, base_logits, indices):
        result = []
        for index in indices:
            residual = self._encode(features[index])[-1]
            result.append(int(self.np.argmax(base_logits[index] + residual)))
        return self.np.asarray(result, dtype=self.np.int64)

    def train_epoch(
        self,
        features,
        base_logits,
        labels,
        indices,
        *,
        class_weights,
        learning_rate: float,
        weight_decay: float,
        clip_norm: float,
    ) -> float:
        np = self.np
        order = np.asarray(indices, dtype=np.int64).copy()
        self.rng.shuffle(order)
        loss_total = 0.0
        for index in order:
            y = int(labels[index])
            ids, weights, z, h, residual = self._encode(features[index])
            logits = base_logits[index] + residual
            shifted = logits - logits.max()
            exp = np.exp(shifted)
            probs = exp / exp.sum()
            sample_weight = float(class_weights[y])
            loss_total += -math.log(max(float(probs[y]), 1e-12)) * sample_weight

            grad_logits = probs.copy()
            grad_logits[y] -= 1.0
            grad_logits *= sample_weight

            grad_w2 = np.outer(h, grad_logits) + weight_decay * self.w2
            grad_b2 = grad_logits
            grad_h = self.w2 @ grad_logits
            grad_a = grad_h * (1.0 - h * h)
            grad_w1 = np.outer(z, grad_a) + weight_decay * self.w1
            grad_b1 = grad_a
            grad_z = self.w1 @ grad_a

            if clip_norm > 0.0:
                sq = (
                    float((grad_w2 * grad_w2).sum())
                    + float((grad_w1 * grad_w1).sum())
                    + float((grad_b2 * grad_b2).sum())
                    + float((grad_b1 * grad_b1).sum())
                    + float((grad_z * grad_z).sum())
                )
                scale = min(1.0, clip_norm / max(math.sqrt(sq), 1e-12))
                grad_w2 *= scale
                grad_b2 *= scale
                grad_w1 *= scale
                grad_b1 *= scale
                grad_z *= scale

            self.w2 -= learning_rate * grad_w2
            self.b2 -= learning_rate * grad_b2
            self.w1 -= learning_rate * grad_w1
            self.b1 -= learning_rate * grad_b1
            self.embedding[ids] -= learning_rate * weights[:, None] * grad_z[None, :]
        return loss_total / max(1, len(order))


def _class_weights(np, labels, indices, class_count: int):
    counts = np.bincount(labels[indices], minlength=class_count).astype(np.float32)
    weights = np.ones(class_count, dtype=np.float32)
    total = float(counts.sum())
    for index, count in enumerate(counts):
        if count > 0:
            weights[index] = total / (class_count * float(count))
    return weights


def _make_linear(TfidfVectorizer, LogisticRegression, seed: int):
    vectorizer = TfidfVectorizer(
        tokenizer=str.split,
        token_pattern=None,
        lowercase=False,
        ngram_range=(1, 2),
        min_df=2,
        max_features=80000,
        sublinear_tf=True,
    )
    classifier = LogisticRegression(
        solver="lbfgs",
        max_iter=4000,
        class_weight="balanced",
        random_state=seed,
    )
    return vectorizer, classifier


def _fit_linear_logits(
    *,
    np,
    TfidfVectorizer,
    LogisticRegression,
    texts,
    labels,
    train_indices,
    seed: int,
):
    vectorizer, classifier = _make_linear(TfidfVectorizer, LogisticRegression, seed)
    train_texts = [texts[index] for index in train_indices]
    matrix_train = vectorizer.fit_transform(train_texts)
    classifier.fit(matrix_train, labels[train_indices])
    matrix_all = vectorizer.transform(texts)
    logits = classifier.decision_function(matrix_all)
    logits = np.asarray(logits, dtype=np.float32)
    if logits.ndim == 1:
        logits = np.column_stack([-logits, logits]).astype(np.float32)
    return logits


def _macro_f1(f1_score, truth, prediction) -> float:
    return float(f1_score(truth, prediction, average="macro", zero_division=0))


def _select_epochs(
    *,
    np,
    TfidfVectorizer,
    LogisticRegression,
    StratifiedGroupKFold,
    f1_score,
    features,
    texts,
    labels,
    groups,
    outer_train,
    class_count: int,
    args,
    seed: int,
) -> tuple[int, float, float]:
    outer_labels = labels[outer_train]
    outer_groups = groups[outer_train]
    split_count = min(args.inner_folds, len(set(outer_groups.tolist())))
    inner = StratifiedGroupKFold(
        n_splits=split_count,
        shuffle=True,
        random_state=seed,
    )
    inner_train_rel, inner_val_rel = next(
        inner.split(np.zeros(len(outer_train)), outer_labels, outer_groups)
    )
    inner_train = outer_train[inner_train_rel]
    inner_val = outer_train[inner_val_rel]

    base_logits = _fit_linear_logits(
        np=np,
        TfidfVectorizer=TfidfVectorizer,
        LogisticRegression=LogisticRegression,
        texts=texts,
        labels=labels,
        train_indices=inner_train,
        seed=seed,
    )
    base_prediction = np.argmax(base_logits[inner_val], axis=1)
    base_score = _macro_f1(f1_score, labels[inner_val], base_prediction)

    model = ResidualEncoder(
        np,
        buckets=args.buckets,
        embedding_dim=args.embedding_dim,
        hidden_dim=args.hidden_dim,
        classes=class_count,
        seed=seed,
    )
    weights = _class_weights(np, labels, inner_train, class_count)
    best_epoch = 0
    best_score = base_score
    stale = 0
    for epoch in range(1, args.epochs + 1):
        lr = args.learning_rate * (args.lr_decay ** (epoch - 1))
        model.train_epoch(
            features,
            base_logits,
            labels,
            inner_train,
            class_weights=weights,
            learning_rate=lr,
            weight_decay=args.weight_decay,
            clip_norm=args.clip_norm,
        )
        prediction = model.predict(features, base_logits, inner_val)
        score = _macro_f1(f1_score, labels[inner_val], prediction)
        if score > best_score + args.min_delta:
            best_score = score
            best_epoch = epoch
            stale = 0
        else:
            stale += 1
        if stale >= args.patience:
            break
    return best_epoch, base_score, best_score


def _train_residual(
    *,
    np,
    features,
    base_logits,
    labels,
    train_indices,
    class_count: int,
    args,
    epochs: int,
    seed: int,
):
    model = ResidualEncoder(
        np,
        buckets=args.buckets,
        embedding_dim=args.embedding_dim,
        hidden_dim=args.hidden_dim,
        classes=class_count,
        seed=seed,
    )
    weights = _class_weights(np, labels, train_indices, class_count)
    final_loss = 0.0
    for epoch in range(1, epochs + 1):
        lr = args.learning_rate * (args.lr_decay ** (epoch - 1))
        final_loss = model.train_epoch(
            features,
            base_logits,
            labels,
            train_indices,
            class_weights=weights,
            learning_rate=lr,
            weight_decay=args.weight_decay,
            clip_norm=args.clip_norm,
        )
    return model, final_loss


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("--task", default="code_domain")
    parser.add_argument("--classes", nargs="+", required=True)
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=5)
    parser.add_argument("--epochs", type=int, default=24)
    parser.add_argument("--patience", type=int, default=6)
    parser.add_argument("--min-delta", type=float, default=1e-4)
    parser.add_argument("--buckets", type=int, default=8192)
    parser.add_argument("--embedding-dim", type=int, default=32)
    parser.add_argument("--hidden-dim", type=int, default=32)
    parser.add_argument("--max-features", type=int, default=4096)
    parser.add_argument("--learning-rate", type=float, default=0.015)
    parser.add_argument("--lr-decay", type=float, default=0.94)
    parser.add_argument("--weight-decay", type=float, default=5e-5)
    parser.add_argument("--clip-norm", type=float, default=3.0)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    if args.folds < 2 or args.inner_folds < 2:
        raise SystemExit("fold counts must be >= 2")
    if args.epochs < 1 or args.patience < 1:
        raise SystemExit("epochs and patience must be positive")

    (
        np,
        TfidfVectorizer,
        LogisticRegression,
        balanced_accuracy_score,
        classification_report,
        confusion_matrix,
        f1_score,
        StratifiedGroupKFold,
    ) = _require_dependencies()

    rows = _load_dataset(args.dataset_dir, args.families, args.task, args.classes)
    class_to_id = {label: index for index, label in enumerate(args.classes)}
    labels = np.asarray([class_to_id[row["label"]] for row in rows], dtype=np.int64)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)
    texts = [row["text"] for row in rows]
    features = [
        hashed_feature_counts(text, buckets=args.buckets, max_features=args.max_features)
        for text in texts
    ]

    print("model: NevergoneHybridResidual/1")
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(row["label"] for row in rows))
    print(
        "residual_parameters:",
        residual_parameter_count(
            buckets=args.buckets,
            embedding_dim=args.embedding_dim,
            hidden_dim=args.hidden_dim,
            classes=len(args.classes),
        ),
    )
    print(
        "shape:",
        f"buckets={args.buckets}",
        f"embedding={args.embedding_dim}",
        f"hidden={args.hidden_dim}",
        f"max_features={args.max_features}",
    )
    print()

    outer = StratifiedGroupKFold(
        n_splits=args.folds,
        shuffle=True,
        random_state=args.seed,
    )
    base_balanced_scores: list[float] = []
    base_macro_scores: list[float] = []
    hybrid_balanced_scores: list[float] = []
    hybrid_macro_scores: list[float] = []
    all_true: list[int] = []
    all_hybrid: list[int] = []

    dummy_x = np.zeros(len(rows), dtype=np.int8)
    for fold, (train_idx, test_idx) in enumerate(outer.split(dummy_x, labels, groups), 1):
        fold_seed = args.seed + 1000 * fold
        best_epoch, inner_base, inner_best = _select_epochs(
            np=np,
            TfidfVectorizer=TfidfVectorizer,
            LogisticRegression=LogisticRegression,
            StratifiedGroupKFold=StratifiedGroupKFold,
            f1_score=f1_score,
            features=features,
            texts=texts,
            labels=labels,
            groups=groups,
            outer_train=train_idx,
            class_count=len(args.classes),
            args=args,
            seed=fold_seed,
        )

        base_logits = _fit_linear_logits(
            np=np,
            TfidfVectorizer=TfidfVectorizer,
            LogisticRegression=LogisticRegression,
            texts=texts,
            labels=labels,
            train_indices=train_idx,
            seed=fold_seed,
        )
        base_prediction = np.argmax(base_logits[test_idx], axis=1)

        if best_epoch > 0:
            model, final_loss = _train_residual(
                np=np,
                features=features,
                base_logits=base_logits,
                labels=labels,
                train_indices=train_idx,
                class_count=len(args.classes),
                args=args,
                epochs=best_epoch,
                seed=fold_seed,
            )
            hybrid_prediction = model.predict(features, base_logits, test_idx)
        else:
            final_loss = 0.0
            hybrid_prediction = base_prediction.copy()

        truth = labels[test_idx]
        base_balanced = float(balanced_accuracy_score(truth, base_prediction))
        base_macro = _macro_f1(f1_score, truth, base_prediction)
        hybrid_balanced = float(balanced_accuracy_score(truth, hybrid_prediction))
        hybrid_macro = _macro_f1(f1_score, truth, hybrid_prediction)

        base_balanced_scores.append(base_balanced)
        base_macro_scores.append(base_macro)
        hybrid_balanced_scores.append(hybrid_balanced)
        hybrid_macro_scores.append(hybrid_macro)
        all_true.extend(truth.tolist())
        all_hybrid.extend(hybrid_prediction.tolist())

        print(
            f"fold={fold} base_balanced={base_balanced:.4f} base_f1={base_macro:.4f} "
            f"hybrid_balanced={hybrid_balanced:.4f} hybrid_f1={hybrid_macro:.4f} "
            f"epochs={best_epoch} inner_base_f1={inner_base:.4f} "
            f"inner_best_f1={inner_best:.4f} loss={final_loss:.4f}"
        )

    print()
    print(
        f"base balanced: {np.mean(base_balanced_scores):.4f} +/- "
        f"{np.std(base_balanced_scores):.4f}"
    )
    print(
        f"base macro_f1: {np.mean(base_macro_scores):.4f} +/- "
        f"{np.std(base_macro_scores):.4f}"
    )
    print(
        f"hybrid balanced: {np.mean(hybrid_balanced_scores):.4f} +/- "
        f"{np.std(hybrid_balanced_scores):.4f}"
    )
    print(
        f"hybrid macro_f1: {np.mean(hybrid_macro_scores):.4f} +/- "
        f"{np.std(hybrid_macro_scores):.4f}"
    )

    print()
    print("HYBRID CONFUSION MATRIX")
    print("order:", args.classes)
    print(confusion_matrix(all_true, all_hybrid, labels=list(range(len(args.classes)))))
    print()
    print("HYBRID CLASSIFICATION REPORT")
    print(
        classification_report(
            all_true,
            all_hybrid,
            labels=list(range(len(args.classes))),
            target_names=args.classes,
            digits=4,
            zero_division=0,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
